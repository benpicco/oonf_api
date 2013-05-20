
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2013, the olsr.org team - see HISTORY file
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * * Redistributions of source code must retain the above copyright
 *   notice, this list of conditions and the following disclaimer.
 * * Redistributions in binary form must reproduce the above copyright
 *   notice, this list of conditions and the following disclaimer in
 *   the documentation and/or other materials provided with the
 *   distribution.
 * * Neither the name of olsr.org, olsrd nor the names of its
 *   contributors may be used to endorse or promote products derived
 *   from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Visit http://www.olsr.org for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 *
 */

#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/common_types.h"

#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_linkconfig.h"
#include "subsystems/oonf_rfc5444.h"
#include "subsystems/oonf_timer.h"

/* definitions and constants */
#define CFG_LINKSPEED_KEY          "linkspeed"

enum {
  CFG_LINKSPEED_DEFAULT = 0,
};

/* Prototypes */
static int _init(void);
static void _cleanup(void);
static void _parse_strarray(struct strarray *array, const char *ifname,
    void (*set)(struct oonf_linkconfig_data *, const char *),
    const char *key, const char *def_value);
static void _set_tx_speed(struct oonf_linkconfig_data *data, const char *value);
static void _cleanup_database(void);
static void _cb_config_changed(void);

/* memory classes */
static struct oonf_class _network_class = {
  .name = "linkconfig networks",
  .size = sizeof(struct oonf_linkconfig_network),
};

static struct oonf_class _link_class = {
  .name = "linkconfig neighbors",
  .size = sizeof(struct oonf_linkconfig_link),
};

/* subsystem definition */
const struct oonf_linkconfig_data oonf_linkconfig_default = {
  .tx_bitrate = CFG_LINKSPEED_DEFAULT,
};

static struct cfg_schema_entry _linkconfig_if_entries[] = {
  CFG_VALIDATE_LINKSPEED(CFG_LINKSPEED_KEY, "",
      "Sets the link speed on the interface. Consists of a speed in"
      " bits/s (with iso-suffix) and an optional list of addresses (both IP and MAC)",
      .list = true),
};

static struct cfg_schema_section _linkconfig_section = {
  .type = CFG_INTERFACE_SECTION,
  .mode = CFG_INTERFACE_SECTION_MODE,
  .cb_delta_handler = _cb_config_changed,
  .entries = _linkconfig_if_entries,
  .entry_count = ARRAYSIZE(_linkconfig_if_entries),
};

struct oonf_subsystem oonf_linkconfig_subsystem = {
  .init = _init,
  .cleanup = _cleanup,

  .cfg_section = &_linkconfig_section,
};

struct avl_tree oonf_linkconfig_network_tree;

/**
 * Subsystem constructor
 * @return always returns 0
 */
static int
_init(void) {
  oonf_class_add(&_network_class);
  oonf_class_add(&_link_class);

  avl_init(&oonf_linkconfig_network_tree, avl_comp_strcasecmp, false);
  return 0;
}

/**
 * Subsystem destructor
 */
static void
_cleanup(void) {
  struct oonf_linkconfig_network *linknet, *n_it;

  avl_for_each_element_safe(&oonf_linkconfig_network_tree, linknet, _node, n_it) {
    oonf_linkconfig_network_remove(linknet);
  }

  oonf_class_remove(&_link_class);
  oonf_class_remove(&_network_class);
}

/**
 * Add a network wide database entry
 * @param name interface name
 * @return network wide database entry, NULL if out of memory
 */
struct oonf_linkconfig_network *
oonf_linkconfig_network_add(const char *name) {
  struct oonf_linkconfig_network *net;

  net = oonf_linkconfig_network_get(name);
  if (!net) {
    net = oonf_class_malloc(&_network_class);
    if (!net) {
      return NULL;
    }
    net->_node.key = net->name;
    strscpy(net->name, name, IF_NAMESIZE);

    avl_insert(&oonf_linkconfig_network_tree, &net->_node);
    avl_init(&net->_link_tree, avl_comp_netaddr, false);

    memcpy(&net->data, &oonf_linkconfig_default, sizeof(net->data));
  }
  return net;
}

/**
 * Remove a network wide database entry including its
 * link specific entries
 * @param net pointer to network wide database entry
 */
void
oonf_linkconfig_network_remove(struct oonf_linkconfig_network *net) {
  struct oonf_linkconfig_link *linklink, *link_it;

  avl_for_each_element_safe(&net->_link_tree, linklink, _node, link_it) {
    oonf_linkconfig_link_remove(linklink);
  }

  avl_remove(&oonf_linkconfig_network_tree, &net->_node);
  oonf_class_free(&_network_class, net);
}

/**
 * Add a link specific database entry
 * @param net pointer to network wide database entry
 * @param remote remote link mac address or originator IP
 * @return pointer to link database entry, NULL if out of memory
 */
struct oonf_linkconfig_link *
oonf_linkconfig_link_add(struct oonf_linkconfig_network *net,
    struct netaddr *remote) {
  struct oonf_linkconfig_link *lnk;

  lnk = oonf_linkconfig_link_get(net, remote);
  if (!lnk) {
    lnk = oonf_class_malloc(&_link_class);
    if (!lnk) {
      return NULL;
    }

    memcpy(&lnk->remote_mac, remote, sizeof(*remote));
    lnk->_node.key = &lnk->remote_mac;

    lnk->net = net;
    avl_insert(&net->_link_tree, &lnk->_node);

    memcpy(&lnk->data, &oonf_linkconfig_default, sizeof(lnk->data));
  }

  return lnk;
}

/**
 * Remove a link specific database entry
 * @param lnk pointer to link specific database entry
 */
void
oonf_linkconfig_link_remove(struct oonf_linkconfig_link *lnk) {
  avl_remove(&lnk->net->_link_tree, &lnk->_node);
  oonf_class_free(&_link_class, lnk);
}

/**
 * Configuration subsystem validator for linkspeed
 * @param entry
 * @param section_name
 * @param value
 * @param out
 * @return
 */
int
oonf_linkconfig_validate_linkspeed(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  struct human_readable_str sbuf;
  struct netaddr_str nbuf;
  struct netaddr dummy;
  const char *ptr;
  uint64_t speed;

  ptr = str_cpynextword(sbuf.buf, value, sizeof(sbuf));
  if (str_parse_human_readable_number(&speed, sbuf.buf, true)) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s is no valid human readable number",
        value, entry->key.entry, section_name);
    return -1;
  }

  while (ptr) {
    ptr = str_cpynextword(nbuf.buf, ptr, sizeof(nbuf));

    if (netaddr_from_string(&dummy, nbuf.buf) != 0
        || netaddr_get_address_family(&dummy) == AF_UNSPEC) {
      cfg_append_printable_line(out, "Value '%s' for entry '%s'"
          " in section %s is no valid address",
          value, entry->key.entry, section_name);
      return -1;
    }
  }
  return 0;
}

/**
 * Parse user input and add the corresponding database entries
 */
static void
_parse_strarray(struct strarray *array, const char *ifname,
    void (*set)(struct oonf_linkconfig_data *, const char *),
    const char *key __attribute__((unused)), const char *def_value) {
  struct oonf_linkconfig_link *linkentry;
  struct oonf_linkconfig_network *netentry;
  struct netaddr_str nbuf;
  struct netaddr linkmac;
  const char *ptr, *value;
  char valuebuf[40];
  char *entry;

  FOR_ALL_STRINGS(array, entry) {
    ptr = str_cpynextword(valuebuf, entry, sizeof(valuebuf));
    netentry = oonf_linkconfig_network_add(ifname);
    if (!netentry) {
      continue;
    }

    if (def_value) {
      value = def_value;
    }
    else {
      value = valuebuf;
    }

    if (ptr == NULL) {
      /* add network wide data entry */
      set(&netentry->data, value);

      OONF_INFO(LOG_MAIN, "if-wide %s for %s: %s",
          key, ifname, value);
      continue;
    }

    while (ptr) {
      ptr = str_cpynextword(nbuf.buf, ptr, sizeof(nbuf));

      if (netaddr_from_string(&linkmac, nbuf.buf) != 0
          || netaddr_get_address_family(&linkmac) == AF_UNSPEC) {
        break;
      }

      linkentry = oonf_linkconfig_link_add(netentry, &linkmac);
      if (!linkentry) {
        continue;
      }

      set(&linkentry->data, value);
      OONF_INFO(LOG_MAIN, "%s to neighbor %s on %s: %s",
          key, nbuf.buf, ifname, value);
    }
  }
}

/**
 * Parse the string representation of tx_speed and put it into database
 * @param data pointer to database entry
 * @param value string value of tx_speed
 */
static void
_set_tx_speed(struct oonf_linkconfig_data *data, const char *value) {
  uint64_t speed;

  if (str_parse_human_readable_number(&speed, value, true)) {
    return;
  }
  data->tx_bitrate = speed;
}

/**
 * Remove all database entries that are completely default
 */
static void
_cleanup_database(void) {
  struct oonf_linkconfig_network *net, *n_it;
  struct oonf_linkconfig_link *lnk, *l_it;
  avl_for_each_element_safe(&oonf_linkconfig_network_tree, net, _node, n_it) {
    avl_for_each_element_safe(&net->_link_tree, lnk, _node, l_it) {
      if (memcmp(&lnk->data, &oonf_linkconfig_default, sizeof(lnk->data)) == 0) {
        /* everything default */
        oonf_linkconfig_link_remove(lnk);
      }
    }

    if (avl_is_empty(&net->_link_tree)
        && memcmp(&net->data, &oonf_linkconfig_default, sizeof(net->data)) == 0) {
      /* everything default */
      oonf_linkconfig_network_remove(net);
    }
  }
}

/**
 * Parse configuration change
 */
static void
_cb_config_changed(void) {
  struct cfg_entry *entry;

  if (_linkconfig_section.pre) {
    entry = cfg_db_get_entry(_linkconfig_section.pre, CFG_LINKSPEED_KEY);
    if (entry) {
      _parse_strarray(&entry->val, _linkconfig_section.section_name,
          _set_tx_speed, CFG_LINKSPEED_KEY, STRINGIFY(CFG_LINKSPEED_DEFAULT));
    }
  }
  if (_linkconfig_section.post) {
    entry = cfg_db_get_entry(_linkconfig_section.post, CFG_LINKSPEED_KEY);
    if (entry) {
      _parse_strarray(&entry->val, _linkconfig_section.section_name,
          _set_tx_speed, CFG_LINKSPEED_KEY, NULL);
    }
  }

  _cleanup_database();
}

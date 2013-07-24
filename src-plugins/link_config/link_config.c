
/*
 * The olsr.org Optimized Link-State Routing daemon version 2 (olsrd2)
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
#include "config/cfg_schema.h"
#include "config/cfg_validate.h"
#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_interface.h"
#include "subsystems/oonf_layer2.h"

#include "link_config/link_config.h"

/* definitions and constants */
#define CFG_RX_LINKSPEED_KEY          "rx_linkspeed"

enum {
  CFG_RX_LINKSPEED_DEFAULT = 0,
};

/* Prototypes */
static int _init(void);
static void _cleanup(void);
static int _cb_validate_linkdata(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out);
static void _parse_strarray(struct strarray *array, const char *ifname,
    enum oonf_layer2_neighbor_index idx);
static void _cb_config_changed(void);

/* define configuration entries */
#define CFG_VALIDATE_LINKDATA(link_index, p_help, args...)         _CFG_VALIDATE(oonf_layer2_metadata_neigh[link_index].key, "", p_help, .cb_validate = _cb_validate_linkdata, .validate_param = {{ .i32 = { link_index }}}, .list = true, ##args )

static struct cfg_schema_entry _linkconfig_if_entries[] = {
  CFG_VALIDATE_LINKDATA(OONF_LAYER2_NEIGH_RX_BITRATE,
    "Sets the incoming link speed on the interface. Consists of a speed in"
    " bits/s (with iso-prefix) and an optional list of mac addresses of neighbor nodes."),
  CFG_VALIDATE_LINKDATA(OONF_LAYER2_NEIGH_TX_BITRATE,
    "Sets the outgoing link speed on the interface. Consists of a speed in"
    " bits/s (with iso-prefix) and an optional list of mac addresses of neighbor nodes."),
  CFG_VALIDATE_LINKDATA(OONF_LAYER2_NEIGH_SIGNAL,
    "Sets the incoing signal strength on the interface. Consists of a signal strength in"
    " dBm (with iso-prefix) and an optional list of mac addresses of neighbor nodes."),
};

static struct cfg_schema_section _linkconfig_section = {
  .type = CFG_INTERFACE_SECTION,
  .mode = CFG_INTERFACE_SECTION_MODE,
  .cb_delta_handler = _cb_config_changed,
  .entries = _linkconfig_if_entries,
  .entry_count = ARRAYSIZE(_linkconfig_if_entries),
};

struct oonf_subsystem oonf_linkconfig_subsystem = {
  .name = "linkconfig",
  .init = _init,
  .cleanup = _cleanup,

  .cfg_section = &_linkconfig_section,
};

uint32_t _l2_origin_current, _l2_origin_old;

/**
 * Subsystem constructor
 * @return always returns 0
 */
static int
_init(void) {
  _l2_origin_current = oonf_layer2_register_origin();
  _l2_origin_old = oonf_layer2_register_origin();
  return 0;
}

/**
 * Subsystem destructor
 */
static void
_cleanup(void) {
  oonf_layer2_cleanup_origin(_l2_origin_current);
  oonf_layer2_cleanup_origin(_l2_origin_old);
}

/**
 * Configuration subsystem validator for linkdata
 * @param entry
 * @param section_name
 * @param value
 * @param out
 * @return
 */
int
_cb_validate_linkdata(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  struct isonumber_str sbuf;
  struct netaddr_str nbuf;
  const char *ptr;

  /* test if first word is a human readable number */
  ptr = str_cpynextword(sbuf.buf, value, sizeof(sbuf));
  if (cfg_validate_int(out, section_name, entry->key.entry, sbuf.buf,
      INT64_MIN, INT64_MAX, 8,
      oonf_layer2_metadata_neigh[entry->validate_param[0].i32[0]].fraction,
      oonf_layer2_metadata_neigh[entry->validate_param[0].i32[0]].binary)) {
    return -1;
  }

  while (ptr) {
    int8_t af[] = { AF_MAC48, AF_EUI64 };

    /* test if the rest of the words are mac addresses */
    ptr = str_cpynextword(nbuf.buf, ptr, sizeof(nbuf));

    if (cfg_validate_netaddr(out, section_name, entry->key.entry,
        nbuf.buf, false, af, ARRAYSIZE(af))) {
      return -1;
    }
  }
  return 0;
}

/**
 * Overwrite a layer-2 value that is either not set or was
 * set by this plugin.
 * @param data pointer to layer2 data
 * @param value new value
 * @return -1 if value was not overwritten, 0 otherwise
 */
static int
_set_l2value(struct oonf_layer2_data *data, int64_t value) {
  uint32_t origin;

  if (oonf_layer2_has_value(data)) {
    origin = oonf_layer2_get_origin(data);

    if (origin != 0 && origin != _l2_origin_current && origin != _l2_origin_old) {
      return -1;
    }
  }

  oonf_layer2_set_value(data, _l2_origin_current, value);
  return 0;
}

/**
 * Parse user input and add the corresponding database entries
 * @param array pointer to string array
 * @param ifname interface name
 * @param idx layer2 neighbor index
 */
static void
_parse_strarray(struct strarray *array, const char *ifname,
    enum oonf_layer2_neighbor_index idx) {
  struct oonf_layer2_neigh *l2neigh;
  struct oonf_layer2_net *l2net;
  struct oonf_interface_data ifdata;
  struct oonf_interface_data *ifptr;
  struct netaddr_str nbuf;
  struct netaddr linkmac;
  struct isonumber_str hbuf;
  int64_t value;
  char *entry;
  const char *ptr;

  ifptr = oonf_interface_get_data(ifname, &ifdata);
  if (!ifptr) {
    OONF_INFO(LOG_LINK_CONFIG, "Interface '%s' does not exist", ifname);
    return;
  }

  l2net = oonf_layer2_net_add(&ifptr->mac);
  if (l2net == NULL) {
    return;
  }

  strarray_for_each_element(array, entry) {
    ptr = str_cpynextword(hbuf.buf, entry, sizeof(hbuf));
    if (str_from_isonumber_s64(&value, hbuf.buf,
        oonf_layer2_metadata_neigh[idx].fraction,
        oonf_layer2_metadata_neigh[idx].binary)) {
      continue;
    }

    if (ptr == NULL) {
      /* add network wide data entry */
      if (!_set_l2value(&l2net->neighdata[idx], value)) {
        OONF_INFO(LOG_LINK_CONFIG, "if-wide %s for %s: %s",
            oonf_layer2_metadata_neigh[idx].key, ifname, hbuf.buf);
      }
      continue;
    }

    while (ptr) {
      ptr = str_cpynextword(nbuf.buf, ptr, sizeof(nbuf));

      if (netaddr_from_string(&linkmac, nbuf.buf) != 0) {
        break;
      }

      l2neigh = oonf_layer2_neigh_add(l2net, &linkmac);
      if (!l2neigh) {
        continue;
      }

      if (!_set_l2value(&l2neigh->data[idx], value)) {
        OONF_INFO(LOG_LINK_CONFIG, "%s to neighbor %s on %s: %s",
            oonf_layer2_metadata_neigh[idx].key, nbuf.buf, ifname, hbuf.buf);
      }
    }
  }
}

/**
 * Parse configuration change
 */
static void
_cb_config_changed(void) {
  struct cfg_schema_entry *schema_entry;
  enum oonf_layer2_neighbor_index l2idx;
  struct oonf_layer2_neigh *l2neigh;
  struct oonf_layer2_net *l2net;
  struct cfg_entry *entry;
  uint32_t l2origin;
  size_t idx;

  if (_linkconfig_section.post) {
    for (idx = 0; idx < ARRAYSIZE(_linkconfig_if_entries); idx++) {
      schema_entry = &_linkconfig_if_entries[idx];
      l2idx = schema_entry->validate_param[0].i32[0];

      entry = cfg_db_get_entry(_linkconfig_section.post, schema_entry->key.entry);
      if (entry) {
        _parse_strarray(&entry->val, _linkconfig_section.section_name, l2idx);
      }
    }
  }

  /* remove old entries and trigger remove events */
  oonf_layer2_cleanup_origin(_l2_origin_old);

  /* trigger change events */
  avl_for_each_element(&oonf_layer2_net_tree, l2net, _node) {
    for (idx = 0; idx < OONF_LAYER2_NET_COUNT; idx++) {
      if (oonf_layer2_get_origin(&l2net->neighdata[idx]) == _l2_origin_current) {
        oonf_layer2_net_commit(l2net);
        break;
      }
    }

    avl_for_each_element(&l2net->neighbors, l2neigh, _node) {
      for (idx = 0; idx < OONF_LAYER2_NEIGH_COUNT; idx++) {
        if (oonf_layer2_get_origin(&l2neigh->data[idx]) == _l2_origin_current) {
          oonf_layer2_neigh_commit(l2neigh);
          break;
        }
      }
    }
  }

  /* switch l2orgigin numbers */
  l2origin = _l2_origin_old;
  _l2_origin_old = _l2_origin_current;
  _l2_origin_current = l2origin;
}

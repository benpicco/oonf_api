#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/common_types.h"

#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_l2config.h"
#include "subsystems/oonf_rfc5444.h"
#include "subsystems/oonf_timer.h"

/* definitions and constants */
#define CFG_KEY_LINKSPEED "linkspeed"

/* Prototypes */
static int _init(void);
static void _cleanup(void);
static void _parse_strarray(struct strarray *array,
    const char *ifname, bool add);

static void _cb_config_changed(void);

/* memory classes */
static struct oonf_class _network_class = {
  .name = "l2config networks",
  .size = sizeof(struct oonf_l2config_network),
};

static struct oonf_class _link_class = {
  .name = "l2config neighbors",
  .size = sizeof(struct oonf_l2config_link),
};

/* subsystem definition */
static struct cfg_schema_entry _l2config_if_entries[] = {
  CFG_VALIDATE_LINKSPEED(CFG_KEY_LINKSPEED, "",
      "Sets the link speed on the interface. Consists of a speed in"
      " bits/s (with iso-suffix) and an optional list of 48-bit mac addresses",
      .list = true),
};

static struct cfg_schema_section _l2config_section = {
  .type = CFG_INTERFACE_SECTION,
  .mode = CFG_SSMODE_NAMED_MANDATORY,
  .cb_delta_handler = _cb_config_changed,
  .entries = _l2config_if_entries,
  .entry_count = ARRAYSIZE(_l2config_if_entries),
};

struct oonf_subsystem oonf_l2config_subsystem = {
  .init = _init,
  .cleanup = _cleanup,

  .cfg_section = &_l2config_section,
};

struct avl_tree oonf_l2config_network_tree;

static int
_init(void) {
  oonf_class_add(&_network_class);
  oonf_class_add(&_link_class);

  avl_init(&oonf_l2config_network_tree, avl_comp_strcasecmp, false);
  return 0;
}

static void
_cleanup(void) {
  struct oonf_l2config_network *l2net, *n_it;

  avl_for_each_element_safe(&oonf_l2config_network_tree, l2net, _node, n_it) {
    oonf_l2config_network_remove(l2net);
  }

  oonf_class_remove(&_link_class);
  oonf_class_remove(&_network_class);
}

struct oonf_l2config_network *
oonf_l2config_network_add(const char *name) {
  struct oonf_l2config_network *net;

  net = oonf_l2config_network_get(name);
  if (!net) {
    net = oonf_class_malloc(&_network_class);
    if (!net) {
      return NULL;
    }
    net->_node.key = net->name;
    strscpy(net->name, name, IF_NAMESIZE);

    avl_insert(&oonf_l2config_network_tree, &net->_node);

    avl_init(&net->_link_tree, avl_comp_netaddr, false);
  }
  return net;
}

void
oonf_l2config_network_remove(struct oonf_l2config_network *net) {
  struct oonf_l2config_link *l2link, *l2_it;

  avl_for_each_element_safe(&net->_link_tree, l2link, _node, l2_it) {
    oonf_l2config_link_remove(l2link);
  }

  avl_remove(&oonf_l2config_network_tree, &net->_node);
  oonf_class_free(&_network_class, net);
}

struct oonf_l2config_link *
oonf_l2config_link_add(struct oonf_l2config_network *net,
    struct netaddr *remote) {
  struct oonf_l2config_link *l2lnk;

  l2lnk = oonf_l2config_link_get(net, remote);
  if (!l2lnk) {
    l2lnk = oonf_class_malloc(&_link_class);
    if (!l2lnk) {
      return NULL;
    }

    memcpy(&l2lnk->remote_mac, remote, sizeof(*remote));
    l2lnk->_node.key = &l2lnk->remote_mac;

    avl_insert(&net->_link_tree, &net->_node);
  }

  return l2lnk;
}

void
oonf_l2config_link_remove(struct oonf_l2config_link *l2lnk) {
  avl_remove(&l2lnk->net->_link_tree, &l2lnk->_node);
  oonf_class_free(&_link_class, l2lnk);
}

int
oonf_l2config_validate_linkspeed(const struct cfg_schema_entry *entry,
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
        || netaddr_get_address_family(&dummy) != AF_MAC48) {
      cfg_append_printable_line(out, "Value '%s' for entry '%s'"
          " in section %s is no valid ethernet address",
          value, entry->key.entry, section_name);
      return -1;
    }
  }
  return 0;
}

/**
 * Parse a string array and modify the fixed linkspeed database
 */
static void
_parse_strarray(struct strarray *array, const char *ifname, bool add) {
  struct oonf_l2config_link *l2link;
  struct oonf_l2config_network *l2net;
  struct human_readable_str sbuf;
  struct netaddr_str nbuf;
  struct netaddr dummy;
  uint64_t speed;
  const char *ptr;
  char *value;

  FOR_ALL_STRINGS(array, value) {
    ptr = str_cpynextword(sbuf.buf, value, sizeof(sbuf));
    if (str_parse_human_readable_number(&speed, sbuf.buf, true)) {
      continue;
    }

    l2net = oonf_l2config_network_add(ifname);
    if (!l2net) {
      continue;
    }

    if (ptr == NULL) {
      /* add fixed link speed for network */
      l2net->tx_bitrate = speed;

      OONF_INFO(LOG_MAIN, "%s if-wide linkspeed for %s: %lu",
          add ? "add" : "remove",
          ifname, speed);
      continue;
    }

    while (ptr) {
      ptr = str_cpynextword(nbuf.buf, ptr, sizeof(nbuf));

      if (netaddr_from_string(&dummy, nbuf.buf) != 0
          || netaddr_get_address_family(&dummy) != AF_MAC48) {
        break;
      }

      l2link = oonf_l2config_link_add(l2net, &dummy);
      if (!l2link) {
        continue;
      }

      l2link->tx_bitrate = speed;

      OONF_INFO(LOG_MAIN, "%s linkspeed to neighbor %s on %s: %lu",
          add ? "add" : "remove",
          nbuf.buf, ifname, speed);
    }
  }
}

/**
 * Parse configuration change
 */
static void
_cb_config_changed(void) {
  struct cfg_entry *entry;

  if (_l2config_section.pre) {
    entry = cfg_db_get_entry(_l2config_section.pre, CFG_KEY_LINKSPEED);
    if (entry) {
      _parse_strarray(&entry->val, _l2config_section.section_name, false);
    }
  }
  if (_l2config_section.post) {
    entry = cfg_db_get_entry(_l2config_section.post, CFG_KEY_LINKSPEED);
    if (entry) {
      _parse_strarray(&entry->val, _l2config_section.section_name, true);
    }
  }
}

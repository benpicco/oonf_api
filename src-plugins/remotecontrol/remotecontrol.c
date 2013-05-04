
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

#include <stdlib.h>

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "common/string.h"

#include "config/cfg_cmd.h"
#include "config/cfg_db.h"
#include "config/cfg_schema.h"

#include "core/olsr_logging.h"
#include "core/olsr_class.h"
#include "core/olsr_plugins.h"
#include "core/olsr_timer.h"
#include "core/olsr_subsystem.h"
#include "core/os_routing.h"

#include "tools/olsr_cfg.h"
#include "tools/olsr_telnet.h"

#include "remotecontrol/remotecontrol.h"

/* variable definitions */
struct _remotecontrol_cfg {
  struct netaddr_acl acl;
};

struct _remotecontrol_session {
  struct list_entity node;
  struct olsr_telnet_cleanup cleanup;

  uint8_t mask[LOG_MAXIMUM_SOURCES];

  struct os_route route;
};

/* prototypes */
static int _init(void);
static void _cleanup(void);

static enum olsr_telnet_result _cb_handle_resource(struct olsr_telnet_data *data);
static enum olsr_telnet_result _cb_handle_route(struct olsr_telnet_data *data);
static enum olsr_telnet_result _cb_handle_log(struct olsr_telnet_data *data);
static enum olsr_telnet_result _cb_handle_config(struct olsr_telnet_data *data);
static enum olsr_telnet_result _update_logfilter(struct olsr_telnet_data *data,
    uint8_t *mask, const char *current, bool value);

static void _print_memory(struct autobuf *buf);
static void _print_timer(struct autobuf *buf);

static enum olsr_telnet_result _start_logging(struct olsr_telnet_data *data,
    struct _remotecontrol_session *rc_session);
static void _stop_logging(struct olsr_telnet_data *data);

static void _cb_print_log(struct log_handler_entry *,
    struct log_parameters *);

static void _cb_route_finished(struct os_route *, int error);
static void _cb_route_get(struct os_route *filter, struct os_route *route);

static void _cb_config_changed(void);
static struct _remotecontrol_session *
    _get_remotecontrol_session(struct olsr_telnet_data *data);
static void _cb_handle_session_cleanup(struct olsr_telnet_cleanup *cleanup);

/* configuration */
static struct cfg_schema_entry _remotecontrol_entries[] = {
  CFG_MAP_ACL(_remotecontrol_cfg, acl, "acl", "+127.0.0.1\0+::1\0default_reject", "acl for remote control commands"),
};

static struct cfg_schema_section _remotecontrol_section = {
  .type = "remotecontrol",
  .cb_delta_handler = _cb_config_changed,
  .entries = _remotecontrol_entries,
  .entry_count = ARRAYSIZE(_remotecontrol_entries),
};

static struct _remotecontrol_cfg _remotecontrol_config;

/* plugin declaration */
struct oonf_subsystem oonf_remotecontrol_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
 .descr = "OLSRD remote control and debug plugin",
  .author = "Henning Rogge",

  .cfg_section = &_remotecontrol_section,

  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(oonf_remotecontrol_subsystem);

/* command callbacks and names */
static struct olsr_telnet_command _telnet_cmds[] = {
  TELNET_CMD("resources", _cb_handle_resource,
      "\"resources memory\": display information about memory usage\n"
      "\"resources timer\": display information about active timers\n",
      .acl = &_remotecontrol_config.acl),
  TELNET_CMD("log", _cb_handle_log,
      "\"log\":      continuous output of logging to this console\n"
      "\"log show\": show configured logging option for debuginfo output\n"
      "\"log add <severity> <source1> <source2> ...\": Add one or more sources of a defined severity for logging\n"
      "\"log remove <severity> <source1> <source2> ...\": Remove one or more sources of a defined severity for logging\n",
      .acl = &_remotecontrol_config.acl),
  TELNET_CMD("config", _cb_handle_config,
      "\"config commit\":                                   Commit changed configuration\n"
      "\"config revert\":                                   Revert to active configuration\n"
      "\"config schema\":                                   Display all allowed section types of configuration\n"
      "\"config schema <section_type>\":                    Display all allowed entries of one configuration section\n"
      "\"config schema <section_type.key>\":                Display help text for configuration entry\n"
      "\"config load <SOURCE>\":                            Load configuration from a SOURCE\n"
      "\"config save <TARGET>\":                            Save configuration to a TARGET\n"
      "\"config set <section_type>.\":                      Add an unnamed section to the configuration\n"
      "\"config set <section_type>.<key>=<value>\":         Add a key/value pair to an unnamed section\n"
      "\"config set <section_type>[<name>].\":              Add a named section to the configuration\n"
      "\"config set <section_type>[<name>].<key>=<value>\": Add a key/value pair to a named section\n"
      "\"config remove <section_type>.\":                   Remove all sections of a certain type\n"
      "\"config remove <section_type>.<key>\":              Remove a key in an unnamed section\n"
      "\"config remove <section_type>[<name>].\":           Remove a named section\n"
      "\"config remove <section_type>[<name>].<key>\":      Remove a key in a named section\n"
      "\"config get\":                                      Show all section types in database\n"
      "\"config get <section_type>.\":                      Show all named sections of a certain type\n"
      "\"config get <section_type>.<key>\":                 Show the value(s) of a key in an unnamed section\n"
      "\"config get <section_type>[<name>].<key>\":         Show the value(s) of a key in a named section\n"
      "\"config format <FORMAT>\":                          Set the format for loading/saving data\n"
      "\"config format AUTO\":                              Set the format to automatic detection\n",
      .acl = &_remotecontrol_config.acl),
  TELNET_CMD("route", _cb_handle_route,
      "\"route add [src <src-ip>] [gw <gateway ip>] dst <destination prefix> [table <table-id>]\n"
      "            [proto <protocol-id>] [metric <metric>] if <if-name>\n"
      "                                                     Set a route in the kernel routing table\n"
      "\"route del [src <src-ip>] [gw <gateway ip>] dst <destination prefix> [table <table-id>]\n"
      "               [proto <protocol-id>] [metric <metric>] if <if-name>\n"
      "                                                     Remove a route in the kernel routing table\n"
      "\"route get [src <src-ip>] [gw <gateway ip>] [dst <destination prefix>] [table <table-id>]\n"
      "               [proto <protocol-id>] [metric <metric>] [if <if-name>] [ipv6]\n"
      "                                                     Lists all known kernel routes matching a set of data\n",
      .acl = &_remotecontrol_config.acl),
};

/* list of telnet sessions with logging mask data */
static struct list_entity _remote_sessions;

/**
 * Initialize remotecontrol plugin
 * @return always returns 0 (cannot fail)
 */
static int
_init(void)
{
  size_t i;

  netaddr_acl_add(&_remotecontrol_config.acl);
  list_init_head(&_remote_sessions);

  for (i=0; i<ARRAYSIZE(_telnet_cmds); i++) {
    olsr_telnet_add(&_telnet_cmds[i]);
  }

  return 0;
}

/**
 * Free all resources of remotecontrol plugin
 */
static void
_cleanup(void)
{
  struct _remotecontrol_session *session, *it;
  size_t i;

  /* shutdown all running logging streams */
  list_for_each_element_safe(&_remote_sessions, session, node, it) {
    olsr_telnet_stop(session->cleanup.data);
  }

  for (i=0; i<ARRAYSIZE(_telnet_cmds); i++) {
    olsr_telnet_remove(&_telnet_cmds[i]);
  }

  netaddr_acl_remove(&_remotecontrol_config.acl);
}

/**
 * Print current resources known to memory manager
 * @param buf output buffer
 */
static void
_print_memory(struct autobuf *buf) {
  struct olsr_class *c;

  avl_for_each_element(&olsr_classes, c, _node) {
    abuf_appendf(buf, "%-25s (MEMORY) size: %"PRINTF_SIZE_T_SPECIFIER
        " usage: %u freelist: %u allocations: %u/%u\n",
        c->name, c->size,
        olsr_class_get_usage(c),
        olsr_class_get_free(c),
        olsr_class_get_allocations(c),
        olsr_class_get_recycled(c));
  }
}

/**
 * Print current resources known to timer scheduler
 * @param buf output buffer
 */
static void
_print_timer(struct autobuf *buf) {
  struct olsr_timer_info *t, *iterator;

  OLSR_FOR_ALL_TIMERS(t, iterator) {
    abuf_appendf(buf, "%-25s (TIMER) usage: %u changes: %u\n",
        t->name, t->usage, t->changes);
  }
}

/**
 * Handle resource command
 * @param data pointer to telnet data
 * @return telnet result constant
 */
static enum olsr_telnet_result
_cb_handle_resource(struct olsr_telnet_data *data) {
  if (data->parameter == NULL || strcasecmp(data->parameter, "memory") == 0) {
    abuf_puts(data->out, "Memory cookies:\n");
    _print_memory(data->out);
  }

  if (data->parameter == NULL || strcasecmp(data->parameter, "timer") == 0) {
    abuf_puts(data->out, "\nTimer cookies:\n");
    _print_timer(data->out);
  }
  return TELNET_RESULT_ACTIVE;
}

/**
 * Update the remotecontrol logging filter
 * @param data pointer to telnet data
 * @param mask pointer to logging mask to manipulate
 * @param param parameters of log add/log remove command
 * @param value true if new source should be added, false
 *    if it should be removed
 * @return telnet result constant
 */
static enum olsr_telnet_result
_update_logfilter(struct olsr_telnet_data *data,
    uint8_t *mask, const char *param, bool value) {
  const char *next;
  enum log_source src;
  enum log_severity sev;

  OLSR_FOR_ALL_LOGSEVERITIES(sev) {
    if ((next = str_hasnextword(param, LOG_SEVERITY_NAMES[sev])) != NULL) {
      break;
    }
  }
  if (sev == LOG_SEVERITY_MAX) {
    abuf_appendf(data->out, "Error, unknown severity level: %s\n", param);
    return TELNET_RESULT_ACTIVE;
  }

  param = next;
  while (param && *param) {
    for (src = 0; src < olsr_log_get_sourcecount(); src++) {
      if ((next = str_hasnextword(param, LOG_SOURCE_NAMES[src])) != NULL) {
        if (value) {
          olsr_log_mask_set(mask, src, sev);
        }
        else {
          olsr_log_mask_reset(mask, src, sev);
        }
        break;
      }
    }
    if (src == olsr_log_get_sourcecount()) {
      abuf_appendf(data->out, "Error, unknown logging source: %s\n", param);
      return TELNET_RESULT_ACTIVE;
    }
    param = next;
  }

  olsr_log_updatemask();
  return TELNET_RESULT_ACTIVE;
}

/**
 * Log handler for telnet output
 * @param entry logging handler
 * @param param logging parameter set
 */
static void
_cb_print_log(struct log_handler_entry *h __attribute__((unused)),
    struct log_parameters *param) {
  struct olsr_telnet_data *data = h->custom;

  abuf_puts(data->out, param->buffer);
  abuf_puts(data->out, "\n");

  /* This might trigger logging output in olsr_socket_stream ! */
  olsr_telnet_flush_session(data);
}

/**
 * Stop handler for continous logging output
 * @param telnet pointer ot telnet telnet
 */
static void
_stop_logging(struct olsr_telnet_data *session) {
  struct log_handler_entry *log_handler;

  log_handler = session->stop_data[0];

  olsr_log_removehandler(log_handler);
  free (log_handler);

  session->stop_handler = NULL;
}

/**
 * Activate logging handler for telnet output
 * @param data pointer to telnet data
 * @param rc_session pointer to remotecontrol session
 * @return telnet result code
 */
static enum olsr_telnet_result
_start_logging(struct olsr_telnet_data *data,
    struct _remotecontrol_session *rc_session) {
  struct log_handler_entry *log_handler;

  log_handler = calloc(1, sizeof(*log_handler));
  if (log_handler == NULL) {
    return TELNET_RESULT_INTERNAL_ERROR;
  }

  olsr_log_mask_copy(log_handler->user_bitmask, rc_session->mask);
  log_handler->custom = data;
  log_handler->handler = _cb_print_log;

  data->stop_handler = _stop_logging;
  data->stop_data[0] = log_handler;

  return TELNET_RESULT_CONTINOUS;
}

/**
 * Handle resource command
 * @param data pointer to telnet data
 * @return telnet result constant
 */
static enum olsr_telnet_result
_cb_handle_log(struct olsr_telnet_data *data) {
  struct _remotecontrol_session *rc_session;
  const char *next;
  enum log_source src;

  rc_session = _get_remotecontrol_session(data);
  if (rc_session == NULL) {
    return TELNET_RESULT_INTERNAL_ERROR;
  }

  if (data->parameter == NULL) {
    if (data->stop_handler) {
      abuf_puts(data->out, "Error, you cannot stack continuous output commands\n");
      return TELNET_RESULT_ACTIVE;
    }

    return _start_logging(data, rc_session);
  }

  if (strcasecmp(data->parameter, "show") == 0) {
    abuf_appendf(data->out, "%*s %*s %*s %*s\n",
        (int)olsr_log_get_max_sourcetextlen(), "",
        (int)olsr_log_get_max_severitytextlen(), LOG_SEVERITY_NAMES[LOG_SEVERITY_DEBUG],
        (int)olsr_log_get_max_severitytextlen(), LOG_SEVERITY_NAMES[LOG_SEVERITY_INFO],
        (int)olsr_log_get_max_severitytextlen(), LOG_SEVERITY_NAMES[LOG_SEVERITY_WARN]);

    for (src=0; src<olsr_log_get_sourcecount(); src++) {
      abuf_appendf(data->out, "%*s %*s %*s %*s\n",
          (int)olsr_log_get_max_sourcetextlen(), LOG_SOURCE_NAMES[src],
          (int)olsr_log_get_max_severitytextlen(),
          olsr_log_mask_test(rc_session->mask, src, LOG_SEVERITY_DEBUG) ? "*" : "",
          (int)olsr_log_get_max_severitytextlen(),
          olsr_log_mask_test(rc_session->mask, src, LOG_SEVERITY_INFO) ? "*" : "",
          (int)olsr_log_get_max_severitytextlen(),
          olsr_log_mask_test(rc_session->mask, src, LOG_SEVERITY_WARN) ? "*" : "");
    }
    return TELNET_RESULT_ACTIVE;
  }

  if ((next = str_hasnextword(data->parameter, "add")) != NULL) {
    return _update_logfilter(data, rc_session->mask, next, true);
  }
  if ((next = str_hasnextword(data->parameter, "remove")) != NULL) {
    return _update_logfilter(data, rc_session->mask, next, false);
  }

  abuf_appendf(data->out, "Error, unknown subcommand for %s: %s",
      data->command, data->parameter);
  return TELNET_RESULT_ACTIVE;
}

/**
 * Handle config command
 * @param data pointer to telnet data
 * @return telnet result constant
 */
static enum olsr_telnet_result
_cb_handle_config(struct olsr_telnet_data *data) {
  const char *next = NULL;

  if (data->parameter == NULL || *data->parameter == 0) {
    abuf_puts(data->out, "Error, 'config' needs a parameter\n");
    return TELNET_RESULT_ACTIVE;
  }

  if ((next = str_hasnextword(data->parameter, "commit"))) {
    if (cfg_schema_validate(olsr_cfg_get_rawdb(),
        false, true, data->out) == 0) {
      olsr_cfg_trigger_commit();
    }
  }
  else if ((next = str_hasnextword(data->parameter, "rollback"))) {
    olsr_cfg_rollback();
  }
  else if ((next = str_hasnextword(data->parameter, "format"))) {
    cfg_cmd_handle_format(olsr_cfg_get_instance(), next);
  }
  else if ((next = str_hasnextword(data->parameter, "get"))) {
    cfg_cmd_handle_get(olsr_cfg_get_instance(),
        olsr_cfg_get_rawdb(), next, data->out);
  }
  else if ((next = str_hasnextword(data->parameter, "load"))) {
    cfg_cmd_handle_load(olsr_cfg_get_instance(),
        olsr_cfg_get_rawdb(), next, data->out);
  }
  else if ((next = str_hasnextword(data->parameter, "remove"))) {
    cfg_cmd_handle_remove(olsr_cfg_get_instance(),
        olsr_cfg_get_rawdb(), next, data->out);
  }
  else if ((next = str_hasnextword(data->parameter, "save"))) {
    cfg_cmd_handle_save(olsr_cfg_get_instance(),
        olsr_cfg_get_rawdb(), next, data->out);
  }
  else if ((next = str_hasnextword(data->parameter, "schema"))) {
    cfg_cmd_handle_schema(
        olsr_cfg_get_rawdb(), next, data->out);
  }
  else if ((next = str_hasnextword(data->parameter, "set"))) {
    cfg_cmd_handle_set(olsr_cfg_get_instance(),
        olsr_cfg_get_rawdb(), next, data->out);
  }
  else {
    abuf_appendf(data->out, "Error, unknown subcommand for %s: %s",
        data->command, data->parameter);
  }

  return TELNET_RESULT_ACTIVE;
}

/**
 * Handle interrupt from user console during route output
 * @param session
 */
static void
_cb_route_stophandler(struct olsr_telnet_data *data) {
  struct _remotecontrol_session *session;

  session = data->stop_data[0];
  os_routing_interrupt(&session->route);
}

/**
 * Handle end of incoming route data
 * @param rt pointer to os_route object
 * @param error error code, 0 if 0 error
 */
static void
_cb_route_finished(struct os_route *rt, int error) {
  struct _remotecontrol_session *session;

  session = container_of(rt, struct _remotecontrol_session, route);

  if (error) {
    abuf_appendf(session->cleanup.data->out, "Command failed: %s (%d)\n",
        strerror(error), error);
  }
  else {
    abuf_puts(session->cleanup.data->out, "Command successful\n");
  }

  olsr_telnet_stop(session->cleanup.data);
}

/**
 * Handle incoming route data
 * @param filter pointer to filter for route data
 * @param route pointer to route data
 */
static void
_cb_route_get(struct os_route *filter, struct os_route *route) {
  struct _remotecontrol_session *session;
  struct autobuf *out;
  struct netaddr_str buf;
  char if_buf[IF_NAMESIZE];

  session = container_of(filter, struct _remotecontrol_session, route);
  out = session->cleanup.data->out;

  if (netaddr_get_address_family(&route->dst) != AF_UNSPEC) {
    abuf_appendf(out, "%s ", netaddr_to_string(&buf, &route->dst));
  }
  if (netaddr_get_address_family(&route->gw) != AF_UNSPEC) {
    abuf_appendf(out, "via %s ", netaddr_to_string(&buf, &route->gw));
  }
  if (netaddr_get_address_family(&route->src) != AF_UNSPEC) {
    abuf_appendf(out, "src %s ", netaddr_to_string(&buf, &route->src));
  }
  if (netaddr_get_address_family(&route->dst) == AF_UNSPEC
      && netaddr_get_address_family(&route->gw) == AF_UNSPEC
      && netaddr_get_address_family(&route->src) == AF_UNSPEC) {
    abuf_appendf(out, "%s ", route->family == AF_INET ? "ipv4" : "ipv6");
  }

  if (route->if_index) {
    abuf_appendf(out, "dev %s (%d) ",
        if_indextoname(route->if_index, if_buf), route->if_index);
  }
  if (route->protocol != RTPROT_UNSPEC) {
    abuf_appendf(out, "prot %d ", route->protocol);
  }
  if (route->metric != -1) {
    abuf_appendf(out, "metric %d ", route->metric);
  }
  if (route->table != RT_TABLE_UNSPEC) {
    abuf_appendf(out, "table %d ", route->table);
  }
  abuf_puts(out, "\n");
  olsr_telnet_flush_session(session->cleanup.data);
}

/**
 * Handle the route command
 * @param data pointer to telnet data
 * @return telnet result constant
 */
static enum olsr_telnet_result
_cb_handle_route(struct olsr_telnet_data *data) {
  bool add  = false, del = false, get = false;
  const char *ptr = NULL, *next = NULL;
  struct _remotecontrol_session *session;
  struct netaddr_str buf;
  struct os_route route;
  int result;

  memcpy(&route, &OS_ROUTE_WILDCARD, sizeof(route));

  if ((next = str_hasnextword(data->parameter, "add")) != NULL) {
    add = true;
  }
  else if ((next = str_hasnextword(data->parameter, "del")) != NULL) {
    del = true;
  }
  else if ((next = str_hasnextword(data->parameter, "get")) != NULL) {
    get = true;
  }

  if (add || del || get) {
    ptr = next;
    while (ptr && *ptr) {
      if ((next = str_hasnextword(ptr, "src"))) {
        ptr = str_cpynextword(buf.buf, next, sizeof(buf));
        if (netaddr_from_string(&route.src, buf.buf) != 0
            || (netaddr_get_address_family(&route.src) != AF_INET
                && netaddr_get_address_family(&route.src) != AF_INET6)) {
          abuf_appendf(data->out, "Error, illegal source: %s", buf.buf);
          return TELNET_RESULT_ACTIVE;
        }
        route.family = netaddr_get_address_family(&route.src);
      }
      else if ((next = str_hasnextword(ptr, "gw"))) {
        ptr = str_cpynextword(buf.buf, next, sizeof(buf));
        if (netaddr_from_string(&route.gw, buf.buf) != 0
            || (netaddr_get_address_family(&route.gw) != AF_INET
                && netaddr_get_address_family(&route.gw) != AF_INET6)) {
          abuf_appendf(data->out, "Error, illegal gateway: %s", buf.buf);
          return TELNET_RESULT_ACTIVE;
        }
        route.family = netaddr_get_address_family(&route.gw);
      }
      else if ((next = str_hasnextword(ptr, "dst"))) {
        ptr = str_cpynextword(buf.buf, next, sizeof(buf));
        if (netaddr_from_string(&route.dst, buf.buf) != 0
            || (netaddr_get_address_family(&route.dst) != AF_INET
                && netaddr_get_address_family(&route.dst) != AF_INET6)) {
          abuf_appendf(data->out, "Error, illegal destination: %s", buf.buf);
          return TELNET_RESULT_ACTIVE;
        }
        route.family = netaddr_get_address_family(&route.dst);
      }
      else if ((next = str_hasnextword(ptr, "table"))) {
        ptr = str_cpynextword(buf.buf, next, sizeof(buf));
        route.table = atoi(buf.buf);
      }
      else if ((next = str_hasnextword(ptr, "proto"))) {
        ptr = str_cpynextword(buf.buf, next, sizeof(buf));
        route.protocol = atoi(buf.buf);
      }
      else if ((next = str_hasnextword(ptr, "metric"))) {
        ptr = str_cpynextword(buf.buf, next, sizeof(buf));
        route.table = atoi(buf.buf);
      }
      else if ((next = str_hasnextword(ptr, "if"))) {
        ptr = str_cpynextword(buf.buf, next, sizeof(buf));
        route.if_index = if_nametoindex(buf.buf);
      }
      else if ((next = str_hasnextword(ptr, "ipv6"))) {
        route.family = AF_INET6;
        ptr = next;
      }
      else {
        abuf_appendf(data->out, "Cannot parse remainder of parameter string: %s", ptr);
        return TELNET_RESULT_ACTIVE;
      }
    }
    if ((add||del) && route.if_index == 0) {
      abuf_appendf(data->out, "Missing or unknown interface");
      return TELNET_RESULT_ACTIVE;
    }
    if ((add||del) && netaddr_get_address_family(&route.dst) == AF_UNSPEC) {
      abuf_appendf(data->out, "Error, IPv4 or IPv6 destination mandatory for add/del");
      return TELNET_RESULT_ACTIVE;
    }
    if ((netaddr_get_address_family(&route.src) != AF_UNSPEC && netaddr_get_address_family(&route.src) != route.family)
        || (netaddr_get_address_family(&route.gw) != AF_UNSPEC && netaddr_get_address_family(&route.gw) != route.family)
        || (netaddr_get_address_family(&route.dst) != AF_UNSPEC && netaddr_get_address_family(&route.dst) != route.family)) {
      abuf_appendf(data->out, "Error, IP address types do not match");
      return TELNET_RESULT_ACTIVE;
    }

    if (route.family == AF_UNSPEC) {
      route.family = AF_INET;
    }

    /* allocate permanent route datastructure for continous output */
    session = _get_remotecontrol_session(data);
    if (session == NULL) {
      return TELNET_RESULT_INTERNAL_ERROR;
    }
    memcpy(&session->route, &route, sizeof(route));

    session->route.cb_finished = _cb_route_finished;
    session->route.cb_get = _cb_route_get;

    if (add || del) {
      result = os_routing_set(&session->route, add, true);
    }
    else {
      result = os_routing_query(&session->route);
    }

    if (result) {
      abuf_puts(data->out, "Error while preparing netlink command");
      return TELNET_RESULT_ACTIVE;
    }

    data->stop_handler = _cb_route_stophandler;
    data->stop_data[0] = session;
    return TELNET_RESULT_CONTINOUS;
  }
  abuf_appendf(data->out, "Error, unknown subcommand for %s: %s",
      data->command, data->parameter);
  return TELNET_RESULT_ACTIVE;
}

/**
 * Update configuration of remotecontrol plugin
 */
static void
_cb_config_changed(void) {
  if (cfg_schema_tobin(&_remotecontrol_config, _remotecontrol_section.post,
      _remotecontrol_entries, ARRAYSIZE(_remotecontrol_entries))) {
    OLSR_WARN(LOG_CONFIG, "Could not convert remotecontrol config to bin");
    return;
  }
}

/**
 * Look for remotecontrol session of telnet data. Create one if
 * necessary
 * @param data pointer to telnet data
 * @return remotecontrol session, NULL if an error happened
 */
static struct _remotecontrol_session *
_get_remotecontrol_session(struct olsr_telnet_data *data) {
  struct _remotecontrol_session *cl;

  list_for_each_element(&_remote_sessions, cl, node) {
    if (cl->cleanup.data == data) {
      return cl;
    }
  }

  /* create new telnet */
  cl = calloc(1, sizeof(*cl));
  if (cl == NULL) {
    OLSR_WARN(LOG_PLUGINS, "Not enough memory for remotecontrol session");
    return NULL;
  }

  cl->cleanup.cleanup_handler = _cb_handle_session_cleanup;
  cl->cleanup.custom = cl;
  olsr_telnet_add_cleanup(data, &cl->cleanup);

  /* copy global mask */
  olsr_log_mask_copy(cl->mask, log_global_mask);

  /* add to remote telnet list */
  list_add_tail(&_remote_sessions, &cl->node);

  return cl;
}

/**
 * Cleanup remotecontrol session if telnet session is over
 * @param cleanup pointer to telnet cleanup handler
 */
static void
_cb_handle_session_cleanup(struct olsr_telnet_cleanup *cleanup) {
  struct _remotecontrol_session *session;

  session = cleanup->custom;
  list_remove(&session->node);
  free(session);
}

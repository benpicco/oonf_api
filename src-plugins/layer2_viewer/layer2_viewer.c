
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

#include <stdio.h>

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "common/string.h"
#include "common/template.h"

#include "config/cfg_schema.h"
#include "core/oonf_logging.h"
#include "core/oonf_plugins.h"
#include "subsystems/oonf_clock.h"
#include "subsystems/oonf_interface.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_telnet.h"

#include "layer2_viewer/layer2_viewer.h"

/* keys for template engine */
#define KEY_neighbor "neighbor"
#define KEY_radio "radio"
#define KEY_ifindex "ifindex"
#define KEY_ifid    "ifid"
#define KEY_interface "interface"
#define KEY_lastseen "lastseen"

/* definitions */
struct _l2viewer_config {
  struct netaddr_acl acl;
};

/* prototypes */
static int _init(void);
static void _cleanup(void);

static void _cb_config_changed(void);

static enum oonf_telnet_result _cb_handle_layer2(struct oonf_telnet_data *data);

/* configuration */
static struct cfg_schema_entry _layer2_entries[] = {
  CFG_MAP_ACL(_l2viewer_config, acl, "acl", "default_accept", "acl for layer2 telnet command"),
};

static struct cfg_schema_section _layer2_section = {
  .type = OONF_PLUGIN_GET_NAME(),
  .cb_delta_handler = _cb_config_changed,
  .entries = _layer2_entries,
  .entry_count = ARRAYSIZE(_layer2_entries),
};

static struct _l2viewer_config _config;

/* plugin declaration */
struct oonf_subsystem oonf_layer2_viewer_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONFD layer2 viewer plugin",
  .author = "Henning Rogge",

  .cfg_section = &_layer2_section,

  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(oonf_layer2_viewer_subsystem);

/* telnet command */
static struct oonf_telnet_command _telnet_cmd =
  TELNET_CMD("layer2", _cb_handle_layer2,
      "\"layer2 net\": show data of all known WLAN networks\n"
      "\"layer2 net list\": show a table of all known active WLAN networks\n"
      "\"layer2 net "JSON_TEMPLATE_FORMAT"\": show a json output of all known active WLAN networks\n"
      "\"layer2 net <template>\": show a table of all known active WLAN networks\n"
      "     (use net_full/net_inactive to output all/inactive networks)\n"
      "\"layer2 neigh\": show data of all known WLAN neighbors\n"
      "\"layer2 neigh list\": show a table of all known WLAN neighbors\n"
      "\"layer2 neigh "JSON_TEMPLATE_FORMAT"\": show a json output of all known WLAN neighbors\n"
      "\"layer2 neigh <template>\": show a table of all known WLAN neighbors\n"
      "     (use neigh_full/neigh_inactive to output all/inactive neighbors)\n",
      .acl = &_config.acl);

/* template buffers */
static struct {
  struct netaddr_str neigh_addr;
  struct netaddr_str radio_addr;
  char ifindex[10];
  char interface[IF_NAMESIZE];
  struct human_readable_str lastseen;
  char if_id[33];
  struct human_readable_str network[OONF_LAYER2_NET_COUNT];
  struct human_readable_str neighbor[OONF_LAYER2_NEIGH_COUNT];
} _template_buf;

static struct abuf_template_data _template_neigh_data[5 + OONF_LAYER2_NEIGH_COUNT] = {
  { .key = KEY_neighbor, .value = _template_buf.neigh_addr.buf, .string = true},
  { .key = KEY_radio, .value = _template_buf.radio_addr.buf, .string = true},
  { .key = KEY_ifindex, .value = _template_buf.ifindex },
  { .key = KEY_interface, .value = _template_buf.interface, .string = true },
  { .key = KEY_lastseen, .value = _template_buf.lastseen.buf },
};

static struct abuf_template_data _template_net_data[5 + OONF_LAYER2_NET_COUNT] = {
  { .key = KEY_radio, .value = _template_buf.radio_addr.buf, .string = true},
  { .key = KEY_ifindex, .value = _template_buf.ifindex },
  { .key = KEY_interface, .value = _template_buf.interface, .string = true },
  { .key = KEY_lastseen, .value = _template_buf.lastseen.buf },
  { .key = KEY_ifid, .value = _template_buf.if_id, .string = true},
};

struct _command_params {
  const char *sub;
  const char *tmpl_full;
  const char *tmpl_table;
  const char *headline_table;

  /* set by runtime */
  const char *template;
};

struct _command_params _net_params = {
  .sub = "net",

  .tmpl_full =
      "Radio MAC:    %" KEY_radio     "%\n"
      "If-Index:     %" KEY_ifindex   "%\n"
      "Interface:    %" KEY_interface "%\n"
      "Interf. ID:   %" KEY_ifid      "%\n"
      "Last seen:    %" KEY_lastseen  "% seconds ago\n"
      "Frequency:    %" OONF_LAYER2_NET_FREQUENCY_KEY "%\n"
      "Max. Bitrate: %" OONF_LAYER2_NET_MAX_BITRATE_KEY "%\n"
      "\n",
  .tmpl_table =
      "%"KEY_interface"%\t%"KEY_radio"%\n",

  .headline_table = "If\tRadio\n",
};

struct _command_params _neigh_params = {
  .sub = "neigh",

  .tmpl_full =
      "Neighbor MAC: %" KEY_neighbor  "%\n"
      "Radio MAC:    %" KEY_radio     "%\n"
      "If-Index:     %" KEY_ifindex   "%\n"
      "Interface:    %" KEY_interface "%\n"
      "Last seen:    %" KEY_lastseen  "% seconds ago\n"
      "Signal:       %" OONF_LAYER2_NEIGH_SIGNAL_KEY    "% dBm\n"
      "Rx bitrate:   %" OONF_LAYER2_NEIGH_RX_BITRATE_KEY "%\n"
      "Rx bytes:     %" OONF_LAYER2_NEIGH_RX_BYTES_KEY   "%\n"
      "Rx frames:    %" OONF_LAYER2_NEIGH_RX_FRAMES_KEY "%\n"
      "Tx bitrate:   %" OONF_LAYER2_NEIGH_TX_BITRATE_KEY "%\n"
      "Tx bytes:     %" OONF_LAYER2_NEIGH_TX_BYTES_KEY   "%\n"
      "Tx frames:    %" OONF_LAYER2_NEIGH_TX_FRAMES_KEY "%\n"
      "Tx retries:   %" OONF_LAYER2_NEIGH_TX_RETRIES_KEY "%\n"
      "Tx failed:    %" OONF_LAYER2_NEIGH_TX_FAILED_KEY  "%\n"
      "\n",
  .tmpl_table =
      "%"KEY_interface"%\t%"KEY_radio"%\t%"KEY_neighbor"%\n",

  .headline_table = "  If\tRadio\tNeighbor\n",
};

/**
 * Constructor of plugin
 * @return 0 if initialization was successful, -1 otherwise
 */
static int
_init(void) {
  int i;

  oonf_telnet_add(&_telnet_cmd);

  /* initialize templates */
  for (i=0; i<OONF_LAYER2_NET_COUNT; i++) {
    _template_net_data[5+i].key = oonf_layer2_metadata_net[i].key;
    _template_net_data[5+i].string = false;
    _template_net_data[5+i].value = _template_buf.network[i].buf;
  }
  for (i=0; i<OONF_LAYER2_NEIGH_COUNT; i++) {
    _template_neigh_data[5+i].key = oonf_layer2_metadata_neigh[i].key;
    _template_neigh_data[5+i].string = false;
    _template_neigh_data[5+i].value = _template_buf.neighbor[i].buf;
  }
  return 0;
}

/**
 * Disable plugin
 */
static void
_cleanup(void) {
  oonf_telnet_remove(&_telnet_cmd);
}

static int
_print_value(struct human_readable_str *dst, struct oonf_layer2_data *data,
    const struct oonf_layer2_metadata *meta, bool raw) {
  int64_t value;

  value = oonf_layer2_get_value(data);
  if (str_get_human_readable_s64(dst, value,
      meta->unit, meta->fraction, meta->binary, raw)) {
    return 0;
  }
  return -1;
}

/**
 * Print the data of a layer2 addr to the telnet stream
 * @param out pointer to output stream
 * @param net pointer to layer2 addr data
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init_network_template_value(struct oonf_layer2_net *net, bool raw) {
  int i;

  memset (&_template_buf, 0, sizeof(_template_buf));

  if (NULL == netaddr_to_string(&_template_buf.radio_addr, &net->addr))
    return -1;

  if (net->if_index) {
    sprintf(_template_buf.ifindex, "%u", net->if_index);
    if_indextoname(net->if_index, _template_buf.interface);
  }

  if (net->if_ident[0]) {
    strscpy(_template_buf.if_id, net->if_ident, sizeof(_template_buf.if_id));
  }

  if (net->last_seen) {
    int64_t relative;

    relative = oonf_clock_get_relative(net->last_seen);
    if (NULL == oonf_clock_toIntervalString(&_template_buf.lastseen, -relative)) {
      return -1;
    }
  }


  for (i=0; i<OONF_LAYER2_NET_COUNT; i++) {
    if (oonf_layer2_has_value(&net->data[i])) {
      if (_print_value(&_template_buf.network[i], &net->data[i],
          &oonf_layer2_metadata_net[i],raw)) {
        return -1;
      }
    }
  }
  return 0;
}

/**
 * Print the data of a layer2 neighbor to the telnet stream
 * @param out pointer to output stream
 * @param net pointer to layer2 neighbor data
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init_neighbor_template_value(struct oonf_layer2_neigh *neigh, bool raw) {
  int i;

  if (NULL == netaddr_to_string(&_template_buf.neigh_addr, &neigh->addr))
    return -1;

  if (NULL == netaddr_to_string(&_template_buf.radio_addr, &neigh->network->addr))
    return -1;

  if (neigh->network->if_index) {
    sprintf(_template_buf.ifindex, "%u", neigh->network->if_index);
    if_indextoname(neigh->network->if_index, _template_buf.interface);
  }

  if (neigh->last_seen) {
    int64_t relative;

    relative = oonf_clock_get_relative(neigh->last_seen);
    if (NULL == oonf_clock_toIntervalString(&_template_buf.lastseen, -relative)) {
      return -1;
    }
  }

  for (i=0; i<OONF_LAYER2_NEIGH_COUNT; i++) {
    if (oonf_layer2_has_value(&neigh->data[i])) {
      if (_print_value(&_template_buf.neighbor[i], &neigh->data[i],
          &oonf_layer2_metadata_neigh[i], raw)) {
        return -1;
      }
    }
  }
  return 0;
}

/**
 * Parse a group of subcommands to support filtered/nonfiltered output with
 * full, list, json and custom template mode
 * @param out output buffer
 * @param cmd command stream
 * @param params pointer to subcommand description
 * @return true if one of the subcommands was found, false otherwise
 */
static bool
_parse_mode(struct autobuf *out, const char *cmd, struct _command_params *params) {
  const char *next;

  if ((next = str_hasnextword(cmd, params->sub)) == NULL) {
    return false;
  }

  if (strcasecmp(next, "list") == 0) {
    abuf_puts(out, params->headline_table);
    params->template = params->tmpl_table;
  }
  else if (strcasecmp(next, JSON_TEMPLATE_FORMAT) == 0) {
    params->template = NULL;
  }
  else if (*next == 0) {
    params->template = params->tmpl_full;
  }
  else {
    params->template = next;
  }
  return true;
}

/**
 * Implementation of 'layer2' telnet command
 * @param data pointer to telnet data
 * @return return code for telnet server
 */
static enum oonf_telnet_result
_cb_handle_layer2(struct oonf_telnet_data *data) {
  struct oonf_layer2_net *net;
  struct oonf_layer2_neigh *neigh;
  struct abuf_template_storage *tmpl_storage = NULL;

  if (data->parameter == NULL || *data->parameter == 0) {
    abuf_puts(data->out, "Error, 'layer2' needs a parameter\n");
    return TELNET_RESULT_ACTIVE;
  }

  if (_parse_mode(data->out, data->parameter, &_net_params)) {
    if (_net_params.template) {
      tmpl_storage = abuf_template_init(
        _template_net_data, ARRAYSIZE(_template_net_data), _net_params.template);
      if (tmpl_storage == NULL) {
        return TELNET_RESULT_INTERNAL_ERROR;
      }
    }

    avl_for_each_element(&oonf_layer2_net_tree, net, _node) {
      if (_init_network_template_value(net, _net_params.template == NULL)) {
        free(tmpl_storage);
        return TELNET_RESULT_INTERNAL_ERROR;
      }
      if (_net_params.template) {
        abuf_add_template(data->out, _net_params.template, tmpl_storage);
      }
      else {
        abuf_add_json(data->out, "",
            _template_net_data, ARRAYSIZE(_template_net_data));
      }
    }
  }
  else if (_parse_mode(data->out, data->parameter, &_neigh_params)) {
    if (_neigh_params.template) {
      tmpl_storage = abuf_template_init(
        _template_neigh_data, ARRAYSIZE(_template_neigh_data), _neigh_params.template);
      if (tmpl_storage == NULL) {
        return TELNET_RESULT_INTERNAL_ERROR;
      }
    }

    avl_for_each_element(&oonf_layer2_net_tree, net, _node) {
      avl_for_each_element(&net->neighbors, neigh, _node) {
        if (_init_neighbor_template_value(neigh, _neigh_params.template == NULL)) {
          free(tmpl_storage);
          return TELNET_RESULT_INTERNAL_ERROR;
        }

        if (_neigh_params.template) {
          abuf_add_template(data->out, _neigh_params.template, tmpl_storage);
        }
        else {
          abuf_add_json(data->out, "", _template_neigh_data, ARRAYSIZE(_template_neigh_data));
        }
      }
    }
  }
  else {
    abuf_appendf(data->out, "Error, unknown parameters for %s command: %s\n",
        data->command, data->parameter);
  }

  free(tmpl_storage);
  return TELNET_RESULT_ACTIVE;
}

/**
 * Update configuration of layer2-viewer plugin
 */
static void
_cb_config_changed(void) {
  if (cfg_schema_tobin(&_config, _layer2_section.post,
      _layer2_entries, ARRAYSIZE(_layer2_entries))) {
    OONF_WARN(LOG_LAYER2_VIEWER, "Could not convert layer2_listener config to bin");
    return;
  }
}

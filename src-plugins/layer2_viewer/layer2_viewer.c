
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2012, the olsr.org team - see HISTORY file
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

/* keys for template engine */
#define KEY_neighbor "neighbor"
#define KEY_radio "radio"
#define KEY_ifindex "ifindex"
#define KEY_interface "interface"
#define KEY_active "active"
#define KEY_shortactive "shortactive"
#define KEY_lastseen "lastseen"
#define KEY_ssid "ssid"
#define KEY_frequency "frequency"
#define KEY_signal "signal"
#define KEY_rxbitrate "rxbitrate"
#define KEY_rxbytes "rxbytes"
#define KEY_rxpackets "rxpackets"
#define KEY_txbitrate "txbitrate"
#define KEY_txbytes "txbytes"
#define KEY_txpackets "txpackets"
#define KEY_txretries "txretries"
#define KEY_txfailed "txfailed"

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
struct oonf_subsystem dlep_layer2_viewer_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONFD layer2 viewer plugin",
  .author = "Henning Rogge",

  .cfg_section = &_layer2_section,

  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(dlep_layer2_viewer_subsystem);

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
  struct netaddr_str neighbor;
  struct netaddr_str radio;
  char ifindex[10];
  char interface[IF_NAMESIZE];
  char active[JSON_BOOL_LENGTH];
  char shortactive[2];
  struct fraction_str lastseen;
  char ssid[33];
  struct human_readable_str frequency;
  char signal[7];
  struct human_readable_str rxbitrate;
  struct human_readable_str rxbytes;
  struct human_readable_str rxpackets;
  struct human_readable_str txbitrate;
  struct human_readable_str txbytes;
  struct human_readable_str txpackets;
  struct human_readable_str txretries;
  struct human_readable_str txfailed;
} _template_buf;

static struct abuf_template_data _template_neigh_data[] = {
  { .key = KEY_neighbor, .value = _template_buf.neighbor.buf, .string = true},
  { .key = KEY_radio, .value = _template_buf.radio.buf, .string = true},
  { .key = KEY_ifindex, .value = _template_buf.ifindex },
  { .key = KEY_interface, .value = _template_buf.interface, .string = true },
  { .key = KEY_active, .value = _template_buf.active },
  { .key = KEY_shortactive, .value = _template_buf.shortactive, .string = true },
  { .key = KEY_lastseen, .value = _template_buf.lastseen.buf },
  { .key = KEY_signal, .value = _template_buf.signal },
  { .key = KEY_rxbitrate, .value = _template_buf.rxbitrate.buf },
  { .key = KEY_rxbytes, .value = _template_buf.rxbytes.buf },
  { .key = KEY_rxpackets, .value = _template_buf.rxpackets.buf },
  { .key = KEY_txbitrate, .value = _template_buf.txbitrate.buf },
  { .key = KEY_txbytes, .value = _template_buf.txbytes.buf },
  { .key = KEY_txpackets, .value = _template_buf.txpackets.buf },
  { .key = KEY_txretries, .value = _template_buf.txretries.buf },
  { .key = KEY_txfailed, .value = _template_buf.txfailed.buf },
};

static struct abuf_template_data _template_net_data[] = {
  { .key = KEY_radio, .value = _template_buf.radio.buf, .string = true},
  { .key = KEY_ifindex, .value = _template_buf.ifindex },
  { .key = KEY_interface, .value = _template_buf.interface, .string = true },
  { .key = KEY_active, .value = _template_buf.active },
  { .key = KEY_lastseen, .value = _template_buf.lastseen.buf },
  { .key = KEY_ssid, .value = _template_buf.ssid, .string = true},
  { .key = KEY_frequency, .value = _template_buf.frequency.buf },
};

struct _command_params {
  const char *cmd_full;
  const char *cmd_active;
  const char *cmd_inactive;
  const char *tmpl_full;
  const char *tmpl_table;
  const char *tmpl_filtered_table;
  const char *headline_table;
  const char *headline_filtered_table;

  /* set by runtime */
  const char *template;
  bool active;
  bool inactive;
};

struct _command_params _net_params = {
  .cmd_full = "net_full",
  .cmd_active = "net",
  .cmd_inactive = "net_inactive",

  .tmpl_full =
      "Radio MAC: %" KEY_radio     "%\n"
      "Active:    %" KEY_active    "%\n"
      "If-Index:  %" KEY_ifindex   "%\n"
      "Interface: %" KEY_interface "%\n"
      "SSID:      %" KEY_ssid      "%\n"
      "Last seen: %" KEY_lastseen  "% seconds ago\n"
      "Frequency: %" KEY_frequency "%\n"
      "\n",
  .tmpl_table =
      "%"KEY_shortactive"%%"KEY_interface"%\t%"KEY_radio"%\n",
  .tmpl_filtered_table =
      "%"KEY_interface"%\t%"KEY_radio"%\n",

  .headline_table = "  If\tRadio            \n",
  .headline_filtered_table = "If\tRadio            \n",
};

struct _command_params _neigh_params = {
  .cmd_full = "neigh_full",
  .cmd_active = "neigh",
  .cmd_inactive = "neigh_inactive",

  .tmpl_full =
      "Neighbor MAC: %" KEY_neighbor  "%\n"
      "Active:       %" KEY_active    "%\n"
      "Radio MAC:    %" KEY_radio     "%\n"
      "If-Index:     %" KEY_ifindex   "%\n"
      "Interface:    %" KEY_interface "%\n"
      "Last seen:    %" KEY_lastseen  "% seconds ago\n"
      "Signal:       %" KEY_signal    "% dBm\n"
      "Rx bitrate:   %" KEY_rxbitrate "%\n"
      "Rx bytes:     %" KEY_rxbytes   "%\n"
      "Rx packets:   %" KEY_rxpackets "%\n"
      "Tx bitrate:   %" KEY_txbitrate "%\n"
      "Tx bytes:     %" KEY_txbytes   "%\n"
      "Tx packets:   %" KEY_txpackets "%\n"
      "Tx retries:   %" KEY_txretries "%\n"
      "Tx failed:    %" KEY_txfailed  "%\n"
      "\n",
  .tmpl_table =
      "%"KEY_shortactive"%%"KEY_interface"%\t%"KEY_radio"%\t%"KEY_neighbor"%\n",
  .tmpl_filtered_table =
      "%"KEY_interface"%\t%"KEY_radio"%\t%"KEY_neighbor"%\n",

  .headline_table = "  If\tRadio            \tNeighbor\n",
  .headline_filtered_table = "If\tRadio            \tNeighbor\n",
};

static enum log_source LOG_LAYER2_VIEWER;

/**
 * Constructor of plugin
 * @return 0 if initialization was successful, -1 otherwise
 */
static int
_init(void) {
  LOG_LAYER2_VIEWER = oonf_log_register_source(OONF_PLUGIN_GET_NAME());

  oonf_telnet_add(&_telnet_cmd);
  return 0;
}

/**
 * Disable plugin
 */
static void
_cleanup(void) {
  oonf_telnet_remove(&_telnet_cmd);
}

/**
 * Print the data of a layer2 network to the telnet stream
 * @param out pointer to output stream
 * @param net pointer to layer2 network data
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init_network_template(struct oonf_layer2_network *net, bool raw) {
  memset (&_template_buf, 0, sizeof(_template_buf));

  if (NULL == netaddr_to_string(&_template_buf.radio, &net->radio_id))
    return -1;

  strcpy (_template_buf.active, abuf_json_getbool(net->active));
  strcpy (_template_buf.shortactive, net->active ? "*" : " ");

  if (net->if_index) {
    sprintf(_template_buf.ifindex, "%u", net->if_index);
    if_indextoname(net->if_index, _template_buf.interface);
  }

  if (oonf_layer2_network_has_ssid(net)) {
    strscpy(_template_buf.ssid, net->ssid, sizeof(_template_buf.ssid));
  }

  if (oonf_layer2_network_has_last_seen(net)) {
    int64_t relative;

    relative = oonf_clock_get_relative(net->last_seen);
    if (NULL == oonf_clock_toIntervalString(&_template_buf.lastseen, -relative)) {
      return -1;
    }
  }
  if (oonf_layer2_network_has_frequency(net)) {
    if (NULL == str_get_human_readable_number(
        &_template_buf.frequency, net->frequency, "Hz", 3, false, raw)) {
      return -1;
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
_init_neighbor_template(struct oonf_layer2_neighbor *neigh, bool raw) {
  if (NULL == netaddr_to_string(&_template_buf.neighbor, &neigh->key.neighbor_mac))
    return -1;

  strcpy (_template_buf.active, abuf_json_getbool(neigh->active));
  strcpy (_template_buf.shortactive, neigh->active ? "*" : " ");

  if (NULL == netaddr_to_string(&_template_buf.radio, &neigh->key.radio_mac))
    return -1;

  if (neigh->if_index) {
    sprintf(_template_buf.ifindex, "%u", neigh->if_index);
    if_indextoname(neigh->if_index, _template_buf.interface);
  }

  if (oonf_layer2_neighbor_has_last_seen(neigh)) {
    int64_t relative;

    relative = oonf_clock_get_relative(neigh->last_seen);
    if (NULL == oonf_clock_toIntervalString(&_template_buf.lastseen, -relative)) {
      return -1;
    }
  }

  if (oonf_layer2_neighbor_has_signal(neigh)) {
    sprintf(_template_buf.signal, "%d", neigh->signal_dbm);
  }
  if (oonf_layer2_neighbor_has_rx_bitrate(neigh)) {
    if (NULL == str_get_human_readable_number(
        &_template_buf.rxbitrate, neigh->rx_bitrate, "bit/s", 1, true, raw)) {
      return -1;
    }
  }
  if (oonf_layer2_neighbor_has_rx_bytes(neigh)) {
    if (NULL == str_get_human_readable_number(
        &_template_buf.rxbytes, neigh->rx_bytes, "Byte", 1, true, raw)) {
      return -1;
    }
  }
  if (oonf_layer2_neighbor_has_rx_packets(neigh)) {
    if (NULL == str_get_human_readable_number(
        &_template_buf.rxpackets, neigh->rx_packets, "", 0, true, raw)) {
      return -1;
    }
  }
  if (oonf_layer2_neighbor_has_tx_bitrate(neigh)) {
    if (NULL == str_get_human_readable_number(
        &_template_buf.txbitrate, neigh->tx_bitrate, "bit/s", 1, true, raw)) {
      return -1;
    }
  }
  if (oonf_layer2_neighbor_has_tx_bytes(neigh)) {
    if (NULL == str_get_human_readable_number(
        &_template_buf.txbytes, neigh->tx_bytes, "Byte", 1, true, raw)) {
      return -1;
    }
  }
  if (oonf_layer2_neighbor_has_tx_packets(neigh)) {
    if (NULL == str_get_human_readable_number(
        &_template_buf.txpackets, neigh->tx_packets, "", 0, true, raw)) {
      return -1;
    }
  }
  if (oonf_layer2_neighbor_has_tx_retries(neigh)) {
    if (NULL == str_get_human_readable_number(
        &_template_buf.txretries, neigh->tx_retries, "", 3, true, raw)) {
      return -1;
    }
  }
  if (oonf_layer2_neighbor_has_tx_failed(neigh)) {
    if (NULL == str_get_human_readable_number(
        &_template_buf.txfailed, neigh->tx_failed, "", 3, true, raw)) {
      return -1;
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
  bool filtered;

  filtered = false;
  if ((next = str_hasnextword(cmd, params->cmd_full))) {
    params->active = true;
    params->inactive = true;
  }
  else if ((next = str_hasnextword(cmd, params->cmd_active))) {
    params->active = true;
    filtered = true;
  }
  else if ((next = str_hasnextword(cmd, params->cmd_inactive))) {
    params->inactive = true;
    filtered = true;
  }
  else {
    return false;
  }

  if (strcasecmp(next, "list") == 0) {
    if (filtered) {
      abuf_puts(out, params->headline_filtered_table);
      params->template = params->tmpl_filtered_table;
    }
    else {
      abuf_puts(out, params->headline_table);
      params->template = params->tmpl_table;
    }
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
  struct oonf_layer2_network *net;
  struct oonf_layer2_neighbor *neigh;
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

    avl_for_each_element(&oonf_layer2_network_id_tree, net, _id_node) {
      if (net->active ? _net_params.active : _net_params.inactive) {
        if (_init_network_template(net, _net_params.template == NULL)) {
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
  }
  else if (_parse_mode(data->out, data->parameter, &_neigh_params)) {
    if (_neigh_params.template) {
      tmpl_storage = abuf_template_init(
        _template_neigh_data, ARRAYSIZE(_template_neigh_data), _neigh_params.template);
      if (tmpl_storage == NULL) {
        return TELNET_RESULT_INTERNAL_ERROR;
      }
    }

    avl_for_each_element(&oonf_layer2_neighbor_tree, neigh, _node) {
      if (neigh->active ? _neigh_params.active : _neigh_params.inactive) {
        if (_init_neighbor_template(neigh, _neigh_params.template == NULL)) {
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

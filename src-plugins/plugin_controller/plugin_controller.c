
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

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "config/cfg_schema.h"
#include "core/olsr_logging.h"
#include "core/olsr_plugins.h"
#include "core/olsr_subsystem.h"

#include "core/olsr_cfg.h"
#include "subsystems/olsr_telnet.h"

#include "plugin_controller/plugin_controller.h"

/* definitions */
struct _acl_config {
  struct netaddr_acl acl;
};

/* prototypes */
static int _init(void);
static void _cleanup(void);

static enum olsr_telnet_result _cb_telnet_plugin(struct olsr_telnet_data *data);
static void _cb_config_changed(void);

struct olsr_telnet_command _telnet_commands[] = {
  TELNET_CMD("plugin", _cb_telnet_plugin,
        "control plugins dynamically, parameters are 'list',"
        "'load <plugin>' and 'unload <plugin>'"),
};

/* configuration */
static struct cfg_schema_entry _plugin_controller_entries[] = {
  CFG_MAP_ACL(_acl_config, acl, "acl", "+127.0.0.1 " ACL_DEFAULT_REJECT, "acl for plugin controller"),
};

static struct cfg_schema_section _plugin_controller_section = {
  .type = OONF_PLUGIN_GET_NAME(),
  .cb_delta_handler = _cb_config_changed,
  .entries = _plugin_controller_entries,
  .entry_count = ARRAYSIZE(_plugin_controller_entries),
};

struct _acl_config _config;

/* plugin declaration */
struct oonf_subsystem oonf_plugin_controller_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OLSRD plugin controller plugin",
  .author = "Henning Rogge",

  .cfg_section = &_plugin_controller_section,

  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(oonf_plugin_controller_subsystem);

/**
 * Constructor of plugin
 * @return 0 if initialization was successful, -1 otherwise
 */
static int
_init(void) {
  netaddr_acl_add(&_config.acl);
  _telnet_commands[0].acl = &_config.acl;

  olsr_telnet_add(&_telnet_commands[0]);
  return 0;
}

/**
 * Destructor of plugin
 */
static void
_cleanup(void) {
  olsr_telnet_remove(&_telnet_commands[0]);
  netaddr_acl_remove(&_config.acl);
}

/**
 * Telnet command 'plugin'
 * @param data pointer to telnet data
 * @return telnet command result
 */
static enum olsr_telnet_result
_cb_telnet_plugin(struct olsr_telnet_data *data) {
  struct oonf_subsystem *plugin;
  const char *plugin_name = NULL;

  if (data->parameter == NULL || strcasecmp(data->parameter, "list") == 0) {
    abuf_puts(data->out, "Plugins:\n");

    avl_for_each_element(&olsr_plugin_tree, plugin, _node) {
      abuf_appendf(data->out, "\t%s\n", plugin->name);
    }
    return TELNET_RESULT_ACTIVE;
  }

  plugin_name = strchr(data->parameter, ' ');
  if (plugin_name == NULL) {
    abuf_appendf(data->out, "Error, missing or unknown parameter\n");
  }

  /* skip whitespaces */
  while (isspace(*plugin_name)) {
    plugin_name++;
  }

  plugin = olsr_plugins_get(plugin_name);
  if (str_hasnextword(data->parameter, "load") == NULL) {
    if (plugin != NULL) {
      abuf_appendf(data->out, "Plugin %s already loaded\n", plugin_name);
      return TELNET_RESULT_ACTIVE;
    }
    plugin = olsr_plugins_load(plugin_name);
    if (plugin != NULL) {
      abuf_appendf(data->out, "Plugin %s successfully loaded\n", plugin_name);
    }
    else {
      abuf_appendf(data->out, "Could not load plugin %s\n", plugin_name);
    }
    return TELNET_RESULT_ACTIVE;
  }

  if (plugin == NULL) {
    abuf_appendf(data->out, "Error, could not find plugin '%s'.\n", plugin_name);
    return TELNET_RESULT_ACTIVE;
  }

  if (str_hasnextword(data->parameter, "unload") == NULL) {
    if (olsr_plugins_unload(plugin)) {
      abuf_appendf(data->out, "Could not unload plugin %s\n", plugin_name);
    }
    else {
      abuf_appendf(data->out, "Plugin %s successfully unloaded\n", plugin_name);
    }
    return TELNET_RESULT_ACTIVE;
  }

  abuf_appendf(data->out, "Unknown command '%s %s %s'.\n",
      data->command, data->parameter, plugin_name);
  return TELNET_RESULT_ACTIVE;
}

/**
 * Handler for configuration changes
 */
static void
_cb_config_changed(void) {
  /* generate binary config */
  cfg_schema_tobin(&_config, _plugin_controller_section.post,
      _plugin_controller_entries, ARRAYSIZE(_plugin_controller_entries));
}


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

#include <stdio.h>
#include <stdlib.h>

#include "common/common_types.h"
#include "config/cfg_schema.h"
#include "config/cfg.h"

#include "core/oonf_cfg.h"
#include "core/oonf_logging.h"
#include "core/oonf_plugins.h"
#include "core/oonf_subsystem.h"

/* global config */
struct oonf_config_global config_global;

static struct cfg_instance _oonf_cfg_instance;
static struct cfg_db *_oonf_raw_db = NULL;
static struct cfg_db *_oonf_work_db = NULL;
static struct cfg_schema _oonf_schema;
static bool _first_apply;

/* remember to trigger reload/commit and the running state */
static bool _trigger_reload, _trigger_commit;
static bool _running = true;

/* remember command line arguments */
static char **_argv;
static int _argc;

/* define global configuration template */
static struct cfg_schema_entry global_entries[] = {
  CFG_MAP_BOOL(oonf_config_global, fork, "fork", "no",
      "Set to true to fork daemon into background."),
  CFG_MAP_BOOL(oonf_config_global, failfast, "failfast", "no",
      "Set to true to stop daemon statup if at least one plugin doesn't load."),

  CFG_MAP_STRINGLIST(oonf_config_global, plugin, CFG_GLOBAL_PLUGIN, "",
      "Set list of plugins to be loaded by daemon. Some might need configuration options."),
};

static struct cfg_schema_section global_section = {
  .type = CFG_SECTION_GLOBAL,
  .entries = global_entries,
  .entry_count = ARRAYSIZE(global_entries),
};


/**
 * Initializes the olsrd configuration subsystem
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_cfg_init(int argc, char **argv) {
  struct oonf_subsystem *plugin;

  cfg_add(&_oonf_cfg_instance);

  /* initialize schema */
  cfg_schema_add(&_oonf_schema);
  cfg_schema_add_section(&_oonf_schema, &global_section);

  /* initialize database */
  if ((_oonf_raw_db = cfg_db_add()) == NULL) {
    OONF_WARN(LOG_CONFIG, "Cannot create raw configuration database.");
    cfg_remove(&_oonf_cfg_instance);
    return -1;
  }

  /* initialize database */
  if ((_oonf_work_db = cfg_db_add()) == NULL) {
    OONF_WARN(LOG_CONFIG, "Cannot create configuration database.");
    cfg_db_remove(_oonf_raw_db);
    cfg_remove(&_oonf_cfg_instance);
    return -1;
  }

  cfg_db_link_schema(_oonf_raw_db, &_oonf_schema);

  /* initialize global config */
  memset(&config_global, 0, sizeof(config_global));
  _first_apply = true;
  _trigger_reload = false;
  _trigger_commit = false;

  _argc = argc;
  _argv = argv;

  /* initialize already existing plugins */
  avl_for_each_element(&oonf_plugin_tree, plugin, _node) {
    oonf_subsystem_configure(&_oonf_schema, plugin);
  }
  return 0;
}

/**
 * Cleans up all data allocated by the olsrd configuration subsystem
 */
void
oonf_cfg_cleanup(void) {
  free(config_global.plugin.value);

  cfg_db_remove(_oonf_raw_db);
  cfg_db_remove(_oonf_work_db);

  cfg_remove(&_oonf_cfg_instance);
}

/**
 * Trigger lazy configuration reload
 */
void
oonf_cfg_trigger_reload(void) {
  OONF_DEBUG(LOG_CONFIG, "Config reload triggered");
  _trigger_reload = true;
}

/**
 * @return true if lazy configuration reload was triggered
 */
bool
oonf_cfg_is_reload_set(void) {
  return _trigger_reload;
}

/**
 * Trigger lazy configuration commit
 */
void
oonf_cfg_trigger_commit(void) {
  OONF_DEBUG(LOG_CONFIG, "Config commit triggered");
  _trigger_commit = true;
}

/**
 * @return true if lazy configuration commit was triggered
 */
bool
oonf_cfg_is_commit_set(void) {
  return _trigger_commit;
}

/**
 * Call this function to end OONF because of an error
 */
void
oonf_cfg_exit(void) {
  OONF_DEBUG(LOG_CONFIG, "Trigger shutdown");
  _running = false;
}

/**
 * @return true if OONF is still running, false if mainloop should
 *   end because of an error.
 */
bool
oonf_cfg_is_running(void) {
  return _running;
}

/**
 * Load all plugins that are not already loaded and remove
 * the plugins that are not needed anymore.
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_cfg_loadplugins(void) {
  struct oonf_subsystem *plugin, *plugin_it;
  char *ptr;
  bool found;

  /* load plugins */
  FOR_ALL_STRINGS(&config_global.plugin, ptr) {
    /* ignore empty strings */
    if (*ptr == 0) {
      continue;
    }
    if (oonf_cfg_load_plugin(ptr) == NULL&& config_global.failfast) {
      return -1;
    }
  }

  /* unload all plugins that are not in use anymore */
  avl_for_each_element_safe(&oonf_plugin_tree, plugin, _node, plugin_it) {
    if (plugin->_dlhandle == NULL) {
      /* ignore static plugins */
      continue;
    }

    found = false;

    /* search if plugin should still be active */
    FOR_ALL_STRINGS(&config_global.plugin, ptr) {
      if (oonf_plugins_get(ptr) == plugin) {
        found = true;
        break;
      }
    }

    if (!found) {
      /* if not, unload it (if not static) */
      oonf_plugins_unload(plugin);
    }
  }

  return 0;
}

struct oonf_subsystem *
oonf_cfg_load_plugin(const char *name) {
  struct oonf_subsystem *plugin;

  plugin = oonf_plugins_get(name);
  if (plugin) {
    /* already loaded */
    return plugin;
  }

  plugin = oonf_plugins_load(name);
  if (plugin) {
    oonf_subsystem_configure(&_oonf_schema, plugin);
  }
  return plugin;
}

void
oonf_cfg_unconfigure_plugins(void) {
  struct oonf_subsystem *plugin, *plugin_it;
  avl_for_each_element_safe(&oonf_plugin_tree, plugin, _node, plugin_it) {
    oonf_subsystem_unconfigure(&_oonf_schema, plugin);
  }
}

void
oonf_cfg_initplugins(void) {
  struct oonf_subsystem *plugin;

  avl_for_each_element(&oonf_plugin_tree, plugin, _node) {
    oonf_plugins_call_init(plugin);
  }
}

/**
 * Applies to content of the raw configuration database into the
 * work database and triggers the change calculation.
 * @return 0 if successful, -1 otherwise
 */
int
oonf_cfg_apply(void) {
  struct cfg_db *old_db;
  struct autobuf log;
  int result;

  if (abuf_init(&log)) {
    OONF_WARN(LOG_CONFIG, "Not enough memory for logging autobuffer");
    return -1;
  }

  OONF_INFO(LOG_CONFIG, "Apply configuration");

  /*** phase 1: activate all plugins ***/
  result = -1;
  old_db = NULL;

  if (oonf_cfg_loadplugins()) {
    goto apply_failed;
  }

  /*** phase 2: check configuration and apply it ***/
  /* validate configuration data */
  if (cfg_schema_validate(_oonf_raw_db, false, true, &log)) {
    OONF_WARN(LOG_CONFIG, "Configuration validation failed");
    OONF_WARN_NH(LOG_CONFIG, "%s", abuf_getptr(&log));
    goto apply_failed;
  }

  /* backup old db */
  old_db = _oonf_work_db;

  /* create new configuration database with correct values */
  _oonf_work_db = cfg_db_duplicate(_oonf_raw_db);
  if (_oonf_work_db == NULL) {
    OONF_WARN(LOG_CONFIG, "Not enough memory for duplicating work db");
    _oonf_work_db = old_db;
    old_db = NULL;
    goto apply_failed;
  }

  /* bind schema */
  cfg_db_link_schema(_oonf_work_db, &_oonf_schema);

  /* remove everything not valid */
  cfg_schema_validate(_oonf_work_db, true, false, NULL);

  if (oonf_cfg_update_globalcfg(false)) {
    /* this should not happen at all */
    OONF_WARN(LOG_CONFIG, "Updating global config failed");
    goto apply_failed;
  }

  /* calculate delta and call handlers */
  if (_first_apply) {
    cfg_schema_handle_db_startup_changes(_oonf_work_db);
    _first_apply = false;
  }
  else {
    cfg_schema_handle_db_changes(old_db, _oonf_work_db);
  }

  /* success */
  result = 0;
  _trigger_reload = false;
  _trigger_commit = false;

  /* now get a new working copy of the committed settings */
  cfg_db_remove(_oonf_raw_db);
  _oonf_raw_db = cfg_db_duplicate(_oonf_work_db);
  cfg_db_link_schema(_oonf_raw_db, &_oonf_schema);

apply_failed:
  if (old_db) {
    cfg_db_remove(old_db);
  }

  abuf_free(&log);
  return result;
}

/**
 * Copy work-db into raw-db to roll back changes before commit.
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_cfg_rollback(void) {
  struct cfg_db *db;

  /* remember old db */
  db = _oonf_raw_db;

  OONF_INFO(LOG_CONFIG, "Rollback configuration");

  _oonf_raw_db = cfg_db_duplicate(_oonf_work_db);
  if (_oonf_raw_db == NULL) {
    OONF_WARN(LOG_CONFIG, "Cannot create raw configuration database.");
    _oonf_raw_db = db;
    return -1;
  }

  /* free old db */
  cfg_db_remove(db);
  return 0;
}

/**
 * Update binary copy of global config section
 * @param raw true if data shall be taken from raw database,
 *   false if work-db should be taken as a source.
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_cfg_update_globalcfg(bool raw) {
  struct cfg_named_section *named;

  named = cfg_db_find_namedsection(
      raw ? _oonf_raw_db : _oonf_work_db, CFG_SECTION_GLOBAL, NULL);

  return cfg_schema_tobin(&config_global,
      named, global_entries, ARRAYSIZE(global_entries));
}

/**
 * This function will clear the raw configuration database
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_cfg_clear_rawdb(void) {
  struct cfg_db *db;

  /* remember old db */
  db = _oonf_raw_db;

  /* initialize database */
  if ((_oonf_raw_db = cfg_db_add()) == NULL) {
    OONF_WARN(LOG_CONFIG, "Cannot create raw configuration database.");
    _oonf_raw_db = db;
    return -1;
  }

  /* free old db */
  cfg_db_remove(db);

  cfg_db_link_schema(_oonf_raw_db, &_oonf_schema);
  return 0;
}

/**
 * @return pointer to configuration instance object
 */
struct cfg_instance *
oonf_cfg_get_instance(void) {
  return &_oonf_cfg_instance;
}

/**
 * @return pointer to olsr configuration database
 */
struct cfg_db *
oonf_cfg_get_db(void) {
  return _oonf_work_db;
}

/**
 * @return pointer to olsr raw configuration database
 */
struct cfg_db *
oonf_cfg_get_rawdb(void) {
  return _oonf_raw_db;
}

/**
 * @return pointer to olsr configuration schema
 */
struct cfg_schema *
oonf_cfg_get_schema(void) {
  return &_oonf_schema;
}

/**
 * @return argument counter of original main() function
 */
int
oonf_cfg_get_argc(void) {
  return _argc;
}

/**
 * @return argument vector of original main() function
 */
const char **
oonf_cfg_get_argv(void) {
  return (const char **) _argv;
}

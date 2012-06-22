
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
#include <stdlib.h>

#include "common/common_types.h"
#include "config/cfg_schema.h"
#include "config/cfg.h"

#include "core/olsr_logging.h"
#include "core/olsr_plugins.h"
#include "core/olsr_socket.h"
#include "core/olsr_subsystem.h"

#include "tools/olsr_cfg.h"

/* static prototypes */
static int _cb_validate_global(struct cfg_schema_section *, const char *section_name,
    struct cfg_named_section *, struct autobuf *);

/* global config */
struct olsr_config_global config_global;

static struct cfg_instance _olsr_cfg_instance;
static struct cfg_db *_olsr_raw_db = NULL;
static struct cfg_db *_olsr_work_db = NULL;
static struct cfg_schema _olsr_schema;
static bool _first_apply;

/* remember to trigger reload/commit and the running state */
static bool _trigger_reload, _trigger_commit;
static bool _running = true;

/* remember if initialized or not */
OLSR_SUBSYSTEM_STATE(_cfg_state);

/* define global configuration template */
static struct cfg_schema_section global_section = {
  .type = CFG_SECTION_GLOBAL,
  .cb_validate = _cb_validate_global,
};

static struct cfg_schema_entry global_entries[] = {
  CFG_MAP_BOOL(olsr_config_global, fork, "fork", "no",
      "Set to true to fork daemon into background."),
  CFG_MAP_BOOL(olsr_config_global, failfast, "failfast", "no",
      "Set to true to stop daemon statup if at least one plugin doesn't load."),

  CFG_MAP_STRINGLIST(olsr_config_global, plugin, CFG_GLOBAL_PLUGIN, "",
      "Set list of plugins to be loaded by daemon. Some might need configuration options."),
};

/**
 * Initializes the olsrd configuration subsystem
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_cfg_init(void) {
  if (olsr_subsystem_is_initialized(&_cfg_state))
    return 0;

  cfg_add(&_olsr_cfg_instance);

  /* initialize schema */
  cfg_schema_add(&_olsr_schema);
  cfg_schema_add_section(&_olsr_schema, &global_section,
      global_entries, ARRAYSIZE(global_entries));

  /* initialize database */
  if ((_olsr_raw_db = cfg_db_add()) == NULL) {
    OLSR_WARN(LOG_CONFIG, "Cannot create raw configuration database.");
    cfg_remove(&_olsr_cfg_instance);
    return -1;
  }

  /* initialize database */
  if ((_olsr_work_db = cfg_db_add()) == NULL) {
    OLSR_WARN(LOG_CONFIG, "Cannot create configuration database.");
    cfg_db_remove(_olsr_raw_db);
    cfg_remove(&_olsr_cfg_instance);
    return -1;
  }

  cfg_db_link_schema(_olsr_raw_db, &_olsr_schema);

  /* initialize global config */
  memset(&config_global, 0, sizeof(config_global));
  _first_apply = true;
  _trigger_reload = false;
  _trigger_commit = false;

  olsr_subsystem_init(&_cfg_state);
  return 0;
}

/**
 * Cleans up all data allocated by the olsrd configuration subsystem
 */
void
olsr_cfg_cleanup(void) {
  if (olsr_subsystem_cleanup(&_cfg_state))
    return;

  free(config_global.plugin.value);

  cfg_db_remove(_olsr_raw_db);
  cfg_db_remove(_olsr_work_db);

  cfg_remove(&_olsr_cfg_instance);
}

/**
 * Trigger lazy configuration reload
 */
void
olsr_cfg_trigger_reload(void) {
  OLSR_DEBUG(LOG_CONFIG, "Config reload triggered");
  _trigger_reload = true;
  olsr_socket_stop_scheduler();
}

/**
 * @return true if lazy configuration reload was triggered
 */
bool
olsr_cfg_is_reload_set(void) {
  return _trigger_reload;
}

/**
 * Trigger lazy configuration commit
 */
void
olsr_cfg_trigger_commit(void) {
  OLSR_DEBUG(LOG_CONFIG, "Config commit triggered");
  _trigger_commit = true;
  olsr_socket_stop_scheduler();
}

/**
 * @return true if lazy configuration commit was triggered
 */
bool
olsr_cfg_is_commit_set(void) {
  return _trigger_commit;
}

/**
 * Call this function to end OLSR because of an error
 */
void
olsr_cfg_exit(void) {
  OLSR_DEBUG(LOG_CONFIG, "Trigger shutdown");
  _running = false;
  olsr_socket_stop_scheduler();
}

/**
 * @return true if OLSR is still running, false if mainloop should
 *   end because of an error.
 */
bool
olsr_cfg_is_running(void) {
  return _running;
}

/**
 * Load all plugins that are not already loaded and remove
 * the plugins that are not needed anymore.
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_cfg_loadplugins(void) {
  struct olsr_plugin *plugin, *plugin_it;
  char *ptr;
  bool found;

  /* load plugins */
  FOR_ALL_STRINGS(&config_global.plugin, ptr) {
    /* ignore empty strings */
    if (*ptr == 0) {
      continue;
    }

    if (olsr_plugins_load(ptr) == NULL && config_global.failfast) {
      return -1;
    }
  }

  /* unload all plugins that are not in use anymore */
  OLSR_FOR_ALL_PLUGIN_ENTRIES(plugin, plugin_it) {
    found = false;

    /* search if plugin should still be active */
    FOR_ALL_STRINGS(&config_global.plugin, ptr) {
      if (olsr_plugins_get(ptr) == plugin) {
        found = true;
        break;
      }
    }

    if (!found && !olsr_plugins_is_static(plugin)) {
      /* if not, unload it (if not static) */
      olsr_plugins_unload(plugin);
    }
  }
  return 0;
}

/**
 * Applies to content of the raw configuration database into the
 * work database and triggers the change calculation.
 * @return 0 if successful, -1 otherwise
 */
int
olsr_cfg_apply(void) {
  struct olsr_plugin *plugin, *plugin_it;
  struct cfg_db *old_db;
  struct autobuf log;
  int result;

  if (abuf_init(&log)) {
    OLSR_WARN(LOG_CONFIG, "Not enough memory for logging autobuffer");
    return -1;
  }

  OLSR_INFO(LOG_CONFIG, "Apply configuration");

  /*** phase 1: activate all plugins ***/
  result = -1;
  old_db = NULL;

  if (olsr_cfg_loadplugins()) {
    goto apply_failed;
  }

  /*** phase 2: check configuration and apply it ***/
  /* validate configuration data */
  if (cfg_schema_validate(_olsr_raw_db, false, true, &log)) {
    OLSR_WARN(LOG_CONFIG, "Configuration validation failed");
    OLSR_WARN_NH(LOG_CONFIG, "%s", abuf_getptr(&log));
    goto apply_failed;
  }

  /* backup old db */
  old_db = _olsr_work_db;

  /* create new configuration database with correct values */
  _olsr_work_db = cfg_db_duplicate(_olsr_raw_db);
  if (_olsr_work_db == NULL) {
    OLSR_WARN(LOG_CONFIG, "Not enough memory for duplicating work db");
    _olsr_work_db = old_db;
    old_db = NULL;
    goto apply_failed;
  }

  /* bind schema */
  cfg_db_link_schema(_olsr_work_db, &_olsr_schema);

  /* enable all plugins */
  OLSR_FOR_ALL_PLUGIN_ENTRIES(plugin, plugin_it) {
    if (plugin->_enabled)
      continue;
    if (olsr_plugins_enable(plugin) != 0
        && config_global.failfast) {
      goto apply_failed;
    }
  }

  /* remove everything not valid */
  cfg_schema_validate(_olsr_work_db, true, false, NULL);

  if (olsr_cfg_update_globalcfg(false)) {
    /* this should not happen at all */
    OLSR_WARN(LOG_CONFIG, "Updating global config failed");
    goto apply_failed;
  }

  /* calculate delta and call handlers */
  if (_first_apply) {
    cfg_schema_handle_db_startup_changes(_olsr_work_db);
    _first_apply = false;
  }
  else {
    cfg_schema_handle_db_changes(old_db, _olsr_work_db);
  }

  /* success */
  result = 0;
  _trigger_reload = false;
  _trigger_commit = false;

  /* now get a new working copy of the committed settings */
  cfg_db_remove(_olsr_raw_db);
  _olsr_raw_db = cfg_db_duplicate(_olsr_work_db);
  cfg_db_link_schema(_olsr_raw_db, &_olsr_schema);

apply_failed:
  /* look for loaded but not enabled plugins and unload them */
  OLSR_FOR_ALL_PLUGIN_ENTRIES(plugin, plugin_it) {
    if (plugin->_loaded && !plugin->_enabled) {
      olsr_plugins_unload(plugin);
    }
  }

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
olsr_cfg_rollback(void) {
  struct cfg_db *db;

  /* remember old db */
  db = _olsr_raw_db;

  OLSR_INFO(LOG_CONFIG, "Rollback configuration");

  _olsr_raw_db = cfg_db_duplicate(_olsr_work_db);
  if (_olsr_raw_db == NULL) {
    OLSR_WARN(LOG_CONFIG, "Cannot create raw configuration database.");
    _olsr_raw_db = db;
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
olsr_cfg_update_globalcfg(bool raw) {
  struct cfg_named_section *named;

  named = cfg_db_find_namedsection(
      raw ? _olsr_raw_db : _olsr_work_db, CFG_SECTION_GLOBAL, NULL);

  return cfg_schema_tobin(&config_global,
      named, global_entries, ARRAYSIZE(global_entries));
}

/**
 * This function will clear the raw configuration database
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_cfg_clear_rawdb(void) {
  struct cfg_db *db;

  /* remember old db */
  db = _olsr_raw_db;

  /* initialize database */
  if ((_olsr_raw_db = cfg_db_add()) == NULL) {
    OLSR_WARN(LOG_CONFIG, "Cannot create raw configuration database.");
    _olsr_raw_db = db;
    return -1;
  }

  /* free old db */
  cfg_db_remove(db);

  cfg_db_link_schema(_olsr_raw_db, &_olsr_schema);
  return 0;
}

/**
 * @return pointer to configuration instance object
 */
struct cfg_instance *
olsr_cfg_get_instance(void) {
  return &_olsr_cfg_instance;
}

/**
 * @return pointer to olsr configuration database
 */
struct cfg_db *
olsr_cfg_get_db(void) {
  return _olsr_work_db;
}

/**
 * @return pointer to olsr raw configuration database
 */
struct cfg_db *
olsr_cfg_get_rawdb(void) {
  return _olsr_raw_db;
}

/**
 * @return pointer to olsr configuration schema
 */
struct cfg_schema *
olsr_cfg_get_schema(void) {
  return &_olsr_schema;
}

/**
 * Validates if the settings of the global section are
 * consistent.
 * @param schema pointer to section schema
 * @param section_name name of section
 * @param section pointer to named section of database
 * @param log pointer to logging output buffer
 * @return -1 if section is not valid, 0 otherwise
 */
static int
_cb_validate_global(struct cfg_schema_section *schema __attribute__((unused)),
    const char *section_name __attribute__((unused)),
    struct cfg_named_section *section, struct autobuf *log) {
  struct olsr_config_global config;

  memset(&config, 0, sizeof(config));

  if (cfg_schema_tobin(&config,
        section, global_entries, ARRAYSIZE(global_entries))) {
    cfg_append_printable_line(log, "Could not generate binary template of global section");
    return -1;
  }

  free(config.plugin.value);
  return 0;
}

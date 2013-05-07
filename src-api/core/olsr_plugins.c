
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

#include <assert.h>
#include <dlfcn.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include "common/autobuf.h"
#include "common/list.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/template.h"

#include "core/olsr_libdata.h"
#include "core/olsr_logging.h"
#include "core/olsr_plugins.h"

/* constants */
enum {
  IDX_DLOPEN_LIB,
  IDX_DLOPEN_PATH,
  IDX_DLOPEN_PRE,
  IDX_DLOPEN_PRELIB,
  IDX_DLOPEN_POST,
  IDX_DLOPEN_POSTLIB,
  IDX_DLOPEN_VER,
  IDX_DLOPEN_VERLIB,
};

/*
 * List of paths to look for plugins
 *
 * The elements of the patterns are:
 *
 * %LIB%:  name of the plugin
 * %PATH%: local path (linux: ".")
 *
 * %PRE%:  shared library prefix defined by the app (linux: "lib<app>_")
 * %POST%: shared library postfix defined by the app (linux: ".so")
 * %VER:   version number as defined by the app (e.g. "0.1.0")
 *
 * %PRELIB%: shared library prefix defined by the API (linux: "liboonf_")
 * %POST%:   shared library postfix defined by the app (linux: ".so")
 * %VER:     version number as defined by the app (e.g. "0.1.0")
 */
// TODO: put a "local library path" setting into the configuration
static const char *dlopen_patterns[] = {
  "%PATH%/oonf/%PRE%%LIB%%POST%.%VER%",
  "%PATH%/oonf/%PRELIB%%LIB%%POSTLIB%.%VERLIB%",
  "%PATH%/oonf/%PRE%%LIB%%POST%",
  "%PATH%/oonf/%PRELIB%%LIB%%POSTLIB%",
  "%PATH%/%PRE%%LIB%%POST%.%VER%",
  "%PATH%/%PRELIB%%LIB%%POSTLIB%.%VERLIB%",
  "%PATH%/%PRE%%LIB%%POST%",
  "%PATH%/%PRELIB%%LIB%%POSTLIB%",
  "oonf/%PRE%%LIB%%POST%.%VER%",
  "oonf/%PRELIB%%LIB%%POSTLIB%.%VERLIB%",
  "oonf/%PRE%%LIB%%POST%",
  "oonf/%PRELIB%%LIB%%POSTLIB%",
  "%PRE%%LIB%%POST%.%VER%",
  "%PRELIB%%LIB%%POSTLIB%.%VERLIB%",
  "%PRE%%LIB%%POST%",
  "%PRELIB%%LIB%%POSTLIB%",
};

/* Local functions */
struct avl_tree olsr_plugin_tree;
static bool _plugin_tree_initialized = false;

/* library loading patterns */
static struct abuf_template_data _dlopen_data[] = {
  [IDX_DLOPEN_LIB]     =  { .key = "LIB" },
  [IDX_DLOPEN_PATH]    =  { .key = "PATH", .value = "." },
  [IDX_DLOPEN_PRE]     =  { .key = "PRE" },
  [IDX_DLOPEN_PRELIB]  =  { .key = "PRELIB" },
  [IDX_DLOPEN_POST]    =  { .key = "POST" },
  [IDX_DLOPEN_POSTLIB] =  { .key = "POSTLIB" },
  [IDX_DLOPEN_VER]     =  { .key = "VER" },
  [IDX_DLOPEN_VERLIB]  =  { .key = "VERLIB" },
};

static void _init_plugin_tree(void);
static int _unload_plugin(struct oonf_subsystem *plugin, bool cleanup);
static void *_open_plugin(const char *filename);

/**
 * Initialize the plugin loader system
 */
void
olsr_plugins_init(void) {
  _init_plugin_tree();

  /* load predefined values for dlopen templates */
  _dlopen_data[IDX_DLOPEN_PRE].value =
      olsr_log_get_appdata()->sharedlibrary_prefix;
  _dlopen_data[IDX_DLOPEN_POST].value =
      olsr_log_get_appdata()->sharedlibrary_postfix;
  _dlopen_data[IDX_DLOPEN_VER].value =
      olsr_log_get_appdata()->app_version;

  _dlopen_data[IDX_DLOPEN_PRELIB].value =
      olsr_log_get_libdata()->sharedlibrary_prefix;
  _dlopen_data[IDX_DLOPEN_POSTLIB].value =
      olsr_log_get_libdata()->sharedlibrary_postfix;
  _dlopen_data[IDX_DLOPEN_VERLIB].value =
      olsr_log_get_libdata()->lib_version;
}

/**
 * Disable and unload all plugins
 */
void
olsr_plugins_cleanup(void) {
  struct oonf_subsystem *plugin, *iterator;

  avl_for_each_element_safe(&olsr_plugin_tree, plugin, _node, iterator) {
    _unload_plugin(plugin, true);
  }
}

/**
 * This function is called by the constructor of a plugin to
 * insert the plugin into the global list. It will be called before
 * any subsystem was initialized!
 * @param plugin pointer to plugin definition
 */
void
olsr_plugins_hook(struct oonf_subsystem *plugin) {
  /* make sure plugin tree is initialized */
  _init_plugin_tree();

  /* check if plugin is already in tree */
  if (olsr_plugins_get(plugin->name)) {
    return;
  }

  /* hook static plugin into avl tree */
  plugin->_node.key = plugin->name;
  avl_insert(&olsr_plugin_tree, &plugin->_node);
}

/**
 * Query for a certain plugin name
 * @param libname name of plugin
 * @return pointer to plugin db entry, NULL if not found
 */
struct oonf_subsystem *
olsr_plugins_get(const char *libname) {
  struct oonf_subsystem *plugin;
  char *ptr, memorize = 0;

  /* extract only the filename, without path, prefix or suffix */
  if ((ptr = strrchr(libname, '/')) != NULL) {
    libname = ptr + 1;
  }

  if ((ptr = strstr(libname, "olsrd_")) != NULL) {
    libname = ptr + strlen("olsrd_");
  }

  if ((ptr = strrchr(libname, '.')) != NULL) {
    memorize = *ptr;
    *ptr = 0;
  }

  plugin = avl_find_element(&olsr_plugin_tree, libname, plugin, _node);

  if (ptr) {
    /* restore path */
    *ptr = memorize;
  }
  return plugin;
}

/**
 * Load a plugin and call its initialize callback
 * @param libname the name of the library(file)
 * @return plugin db object
 */
struct oonf_subsystem *
olsr_plugins_load(const char *libname)
{
  void *dlhandle;
  struct oonf_subsystem *plugin;

  /* see if the plugin is there */
  if ((plugin = olsr_plugins_get(libname)) == NULL) {
    /* attempt to load the plugin */
    dlhandle = _open_plugin(libname);

    if (dlhandle == NULL) {
      /* Logging output has already been done by _open_plugin() */
      return NULL;
    }

    /* plugin should be in the tree now */
    if ((plugin = olsr_plugins_get(libname)) == NULL) {
      OLSR_WARN(LOG_PLUGINLOADER, "dynamic library loading failed: \"%s\"!\n", dlerror());
      dlclose(dlhandle);
      return NULL;
    }

    plugin->_dlhandle = dlhandle;

    /* hook into tree */
    plugin->_node.key = plugin->name;
    avl_insert(&olsr_plugin_tree, &plugin->_node);
  }
  return plugin;
}

/**
 * Call the initialization callback of a plugin to activate it
 * @param plugin pointer to plugin db object
 * @return -1 if initialization failed, 0 otherwise
 */
int
olsr_plugins_call_init(struct oonf_subsystem *plugin) {
  if (!plugin->_initialized && plugin->init != NULL) {
    if (plugin->init()) {
      OLSR_WARN(LOG_PLUGINLOADER, "Init callback failed for plugin %s\n", plugin->name);
      return -1;
    }
    OLSR_DEBUG(LOG_PLUGINLOADER, "Load callback of plugin %s successful\n", plugin->name);
  }
  plugin->_initialized = true;
  return 0;
}

/**
 * Unloads an active plugin. Static plugins cannot be removed until
 * final cleanup.
 * @param plugin pointer to plugin db object
 * @return 0 if plugin was removed, -1 otherwise
 */
int
olsr_plugins_unload(struct oonf_subsystem *plugin) {
  return _unload_plugin(plugin, false);
}

/**
 * Initialize plugin tree for early loading of static plugins
 */
static void
_init_plugin_tree(void) {
  if (_plugin_tree_initialized) {
    return;
  }
  avl_init(&olsr_plugin_tree, avl_comp_strcasecmp, false);
  _plugin_tree_initialized = true;
}

/**
 * Internal helper function to unload a plugin using the old API
 * @param plugin pointer to plugin db object
 * @param cleanup true if this is the final cleanup
 *   before OLSR shuts down, false otherwise
 * @return 0 if the plugin was removed, -1 otherwise
 */
static int
_unload_plugin(struct oonf_subsystem *plugin, bool cleanup) {
  if (!plugin->can_cleanup && !cleanup) {
    OLSR_WARN(LOG_PLUGINLOADER, "Plugin %s does not support unloading",
        plugin->name);
    return -1;
  }

  if (plugin->_initialized) {
    OLSR_INFO(LOG_PLUGINLOADER, "Unloading plugin %s\n", plugin->name);

    /* remove first from tree */
    avl_delete(&olsr_plugin_tree, &plugin->_node);

    /* cleanup */
    if (plugin->cleanup) {
      plugin->cleanup();
    }
    if (plugin->_dlhandle) {
      dlclose(plugin->_dlhandle);
    }
  }
  return 0;
}

/**
 * Internal helper to load plugin with different variants of the
 * filename.
 * @param filename pointer to filename
 */
static void *
_open_plugin(const char *filename) {
  struct abuf_template_storage *table;
  struct autobuf abuf;
  void *result;
  size_t i;

  if (abuf_init(&abuf)) {
    OLSR_WARN(LOG_PLUGINLOADER, "Not enough memory for plugin name generation");
    return NULL;
  }

  result = NULL;
  _dlopen_data[IDX_DLOPEN_LIB].value = filename;

  for (i=0; result == NULL && i<ARRAYSIZE(dlopen_patterns); i++) {
    table = abuf_template_init(
        _dlopen_data, ARRAYSIZE(_dlopen_data), dlopen_patterns[i]);

    if (table == NULL) {
      OLSR_WARN(LOG_PLUGINLOADER, "Could not parse pattern %s for dlopen",
          dlopen_patterns[i]);
      continue;
    }

    abuf_clear(&abuf);
    abuf_add_template(&abuf, dlopen_patterns[i], table);
    free(table);

    OLSR_DEBUG(LOG_PLUGINLOADER, "Trying to load library: %s", abuf_getptr(&abuf));
    result = dlopen(abuf_getptr(&abuf), RTLD_NOW);
    if (result == NULL) {
      OLSR_DEBUG(LOG_PLUGINLOADER, "Loading of plugin file %s failed: %s",
          abuf_getptr(&abuf), dlerror());
    }
  }
  if (result == NULL) {
    OLSR_WARN(LOG_PLUGINLOADER, "Loading of plugin %s failed.\n", filename);
  }
  else {
    OLSR_INFO(LOG_PLUGINLOADER, "Loading plugin %s from %s\n", filename, abuf_getptr(&abuf));
  }

  abuf_free(&abuf);
  return result;
}


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

#ifndef _OLSR_PLUGIN_LOADER
#define _OLSR_PLUGIN_LOADER

#include "common/common_types.h"
#include "common/avl.h"
#include "common/list.h"

/*
 * declare a plugin
 *
 * OLSR_PLUGIN7 {
 *   .descr = "<description of the plugin",
 *   .author = "<author of the plugin",
 *   .....
 * };
 */
#define OLSR_PLUGIN7 _OLSR_PLUGIN7_DEF(PLUGIN_FULLNAME)

#define _OLSR_PLUGIN7_DEF(param) _OLSR_PLUGIN7_DEF2(param)
#define _OLSR_PLUGIN7_DEF2(plg_name) static struct olsr_plugin olsr_internal_plugin_definition; \
EXPORT void hookup_plugin_ ## plg_name (void) __attribute__ ((constructor)); \
void hookup_plugin_ ## plg_name (void) { \
  olsr_internal_plugin_definition.name = #plg_name; \
  olsr_plugins_hook(&olsr_internal_plugin_definition); \
} \
static struct olsr_plugin olsr_internal_plugin_definition =

#define OLSR_PLUGIN7_GET_NAME() _OLSR_PLUGIN7_GET_NAME(PLUGIN_FULLNAME)
#define _OLSR_PLUGIN7_GET_NAME(plg_name) #plg_name

/* Representation of a plugin */
struct olsr_plugin {
  struct avl_node p_node;

  /* plugin information */
  const char *name;
  const char *descr;
  const char *author;

  /* true if the plugin can be disables/enabled during runtime */
  bool can_disable;

  /* true if the plugin can be (de)activated during runtime */
  bool can_unload;

  /* plugin callbacks for (de)initialization */
  int (*load) (void);
  int (*enable) (void);
  int (*disable) (void);
  int (*unload) (void);

  /* pointer to dlopen handle */
  void *_dlhandle;

  /* true if the plugin has been loaded */
  bool _loaded;

  /* true if the plugin has been enables */
  bool _enabled;
};

#define OLSR_FOR_ALL_PLUGIN_ENTRIES(plugin, iterator) avl_for_each_element_safe(&plugin_tree, plugin, p_node, iterator)
EXPORT extern struct avl_tree plugin_tree;

EXPORT void olsr_plugins_init(void);
EXPORT void olsr_plugins_cleanup(void);

EXPORT void olsr_plugins_hook(struct olsr_plugin *plugin);
EXPORT int olsr_plugins_init_static(void) __attribute__((warn_unused_result));

EXPORT struct olsr_plugin *olsr_plugins_get(const char *libname);

EXPORT struct olsr_plugin *olsr_plugins_load(const char *);
EXPORT int olsr_plugins_unload(struct olsr_plugin *);
EXPORT int olsr_plugins_enable(struct olsr_plugin *);
EXPORT int olsr_plugins_disable(struct olsr_plugin *);

/**
 * @param p pointer to plugin
 * @return true if its a static plugin, false otherwise
 */
static INLINE bool
olsr_plugins_is_static(struct olsr_plugin *p) {
  return p->_dlhandle == NULL;
}

/**
 * @param p pointer to plugin
 * @return true if its a static plugin, false otherwise
 */
static INLINE bool
olsr_plugins_is_enabled(struct olsr_plugin *p) {
  return p->_enabled;
}

#endif

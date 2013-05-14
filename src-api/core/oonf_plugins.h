
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

#ifndef _OONF_PLUGIN_LOADER
#define _OONF_PLUGIN_LOADER

#include "common/common_types.h"
#include "common/avl.h"
#include "common/list.h"

#include "core/oonf_subsystem.h"

#define DECLARE_OONF_PLUGIN(subsystem) _OONF_PLUGIN_DEF(PLUGIN_FULLNAME, subsystem)

#define _OONF_PLUGIN_DEF(plg_name, subsystem) _OONF_PLUGIN_DEF2(plg_name, subsystem)
#define _OONF_PLUGIN_DEF2(plg_name, subsystem) EXPORT void hookup_plugin_ ## plg_name (void) __attribute__ ((constructor)); void hookup_plugin_ ## plg_name (void) { oonf_plugins_hook(&subsystem); }

#define OONF_PLUGIN_GET_NAME() _OONF_PLUGIN_GET_NAME(PLUGIN_FULLNAME)
#define _OONF_PLUGIN_GET_NAME(plg_name) STRINGIFY(plg_name)

EXPORT extern struct avl_tree oonf_plugin_tree;

EXPORT void oonf_plugins_init(void);
EXPORT void oonf_plugins_cleanup(void);

EXPORT void oonf_plugins_hook(struct oonf_subsystem *subsystem);

EXPORT int oonf_plugins_call_init(struct oonf_subsystem *plugin);
EXPORT void oonf_plugins_call_cleanup(struct oonf_subsystem *plugin);

EXPORT struct oonf_subsystem *oonf_plugins_get(const char *libname);

EXPORT struct oonf_subsystem *oonf_plugins_load(const char *);
EXPORT int oonf_plugins_unload(struct oonf_subsystem *);

#endif

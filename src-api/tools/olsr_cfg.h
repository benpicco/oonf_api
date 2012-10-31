
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

#ifndef OLSR_CFG_H_
#define OLSR_CFG_H_

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "config/cfg_schema.h"

struct olsr_config_global {
  struct strarray plugin;

  bool fork;
  bool failfast;
};

/* section types for configuration */
#define CFG_SECTION_GLOBAL   "global"
#define CFG_GLOBAL_PLUGIN    "plugin"

EXPORT extern struct olsr_config_global config_global;

EXPORT int olsr_cfg_init(int argc, char **argv) __attribute__((warn_unused_result));
EXPORT void olsr_cfg_cleanup(void);
EXPORT int olsr_cfg_loadplugins(void) __attribute__((warn_unused_result));
EXPORT int olsr_cfg_apply(void) __attribute__((warn_unused_result));
EXPORT int olsr_cfg_rollback(void);
EXPORT void olsr_cfg_exit(void);
EXPORT bool olsr_cfg_is_running(void);

EXPORT void olsr_cfg_trigger_reload(void);
EXPORT bool olsr_cfg_is_reload_set(void);
EXPORT void olsr_cfg_trigger_commit(void);
EXPORT bool olsr_cfg_is_commit_set(void);

/* do not export this to plugins */
EXPORT int olsr_cfg_update_globalcfg(bool) __attribute__((warn_unused_result));
EXPORT int olsr_cfg_clear_rawdb(void) __attribute__((warn_unused_result));

EXPORT struct cfg_instance *olsr_cfg_get_instance(void);
EXPORT struct cfg_db *olsr_cfg_get_db(void);
EXPORT struct cfg_db *olsr_cfg_get_rawdb(void);
EXPORT struct cfg_schema *olsr_cfg_get_schema(void);

EXPORT int olsr_cfg_get_argc(void);
EXPORT const char **olsr_cfg_get_argv(void);

#endif /* OLSR_CFG_H_ */

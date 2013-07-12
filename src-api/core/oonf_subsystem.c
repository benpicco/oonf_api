
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

#include <assert.h>

#include "common/common_types.h"
#include "config/cfg_schema.h"

#include "core/oonf_subsystem.h"
#include "core/oonf_logging.h"

void
oonf_subsystem_configure(struct cfg_schema *schema,
    struct oonf_subsystem *subsystem) {
  struct cfg_schema_section *schema_section;

  assert(subsystem->name);

  OONF_INFO(LOG_SUBSYSTEMS, "Configure subsystem %s", subsystem->name);

  /* add schema sections to global schema */
  schema_section = subsystem->cfg_section;
  while (schema_section) {
    OONF_DEBUG(LOG_SUBSYSTEMS, "Add configuration section %s", schema_section->type);

    cfg_schema_add_section(schema, schema_section);
    schema_section = schema_section->next_section;
  }

  if (subsystem->early_cfg_init) {
    OONF_DEBUG(LOG_SUBSYSTEMS, "Call 'early_cfg_init() callback");
    subsystem->early_cfg_init();
  }

  /* add logging source */
  if (!subsystem->no_logging) {
    OONF_DEBUG(LOG_SUBSYSTEMS, "Register logging source %s", subsystem->name);
    subsystem->logging = oonf_log_register_source(subsystem->name);
  }
  else {
    subsystem->logging = LOG_MAIN;
  }
}

void
oonf_subsystem_unconfigure(struct cfg_schema *schema,
    struct oonf_subsystem *subsystem) {
  struct cfg_schema_section *schema_section;

  OONF_INFO(LOG_SUBSYSTEMS, "Unregister subsystem %s", subsystem->name);

  schema_section = subsystem->cfg_section;
  while (schema_section) {
    OONF_DEBUG(LOG_SUBSYSTEMS, "Unregister configuration section %s", schema_section->type);
    cfg_schema_remove_section(schema, schema_section);
    schema_section = schema_section->next_section;
  }
}


/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2021, the olsr.org team - see HISTORY file
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

#ifndef OLSR_H_
#define OLSR_H_

#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "common/common_types.h"

/* variable for subsystem state */
#define OLSR_SUBSYSTEM_STATE(var_name) static bool var_name = false

/**
 * Subsystem marker API for 'being initialized' state.
 * Call this function at the beginning of the initialization.
 * @param ptr pointer to initialized state variable of subsystem
 * @return true if initialization should be skipped, false otherwise
 */
static INLINE bool
olsr_subsystem_init(bool *ptr) {
  if (*ptr)
    return true;

  *ptr = true;
  return false;
}

/**
 * Subsystem marker API for 'being initialized' state.
 * Call this function at the beginning of the cleanup.
 * @param ptr pointer to initialized state variable of subsystem
 * @return true if cleanup should be skipped, false otherwise
 */
static INLINE bool
olsr_subsystem_cleanup(bool *ptr) {
  if (*ptr) {
    *ptr = false;
    return false;
  }
  return true;
}

/**
 * Subsystem marker API for 'being initialized' state.
 * @param ptr pointer to initialized state variable of subsystem
 * @return true if the subsystem is initialized
 */
static INLINE bool
olsr_subsystem_is_initialized(bool *ptr) {
  return *ptr;
}
#endif /* OLSR_H_ */


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

#ifndef OONF_DUPLICATE_SET_H_
#define OONF_DUPLICATE_SET_H_

#include "common/avl.h"
#include "common/common_types.h"
#include "common/netaddr.h"
#include "subsystems/oonf_timer.h"

enum { OONF_DUPSET_MAXIMUM_TOO_OLD = 8 };

enum oonf_duplicate_result {
  OONF_DUPSET_TOO_OLD,
  OONF_DUPSET_DUPLICATE,
  OONF_DUPSET_CURRENT,
  OONF_DUPSET_NEW,
  OONF_DUPSET_NEWEST,

  /* this one must be the last entry */
  OONF_DUPSET_MAX,
};

struct oonf_duplicate_set {
  struct avl_tree _tree;
};

struct oonf_duplicate_entry_key {
  struct netaddr addr;
  uint8_t  msg_type;
};

struct oonf_duplicate_entry {
  struct oonf_duplicate_entry_key key;

  uint32_t history;
  uint16_t current;

  uint16_t too_old_count;

  struct oonf_duplicate_set *set;

  struct avl_node _node;
  struct oonf_timer_entry _vtime;
};

#define LOG_DUPLICATE_SET oonf_duplicate_set_subsystem.logging
EXPORT extern struct oonf_subsystem oonf_duplicate_set_subsystem;
EXPORT extern const char *OONF_DUPSET_RESULT_STR[OONF_DUPSET_MAX];

EXPORT void oonf_duplicate_set_add(struct oonf_duplicate_set *);
EXPORT void oonf_duplicate_set_remove(struct oonf_duplicate_set *);

EXPORT enum oonf_duplicate_result oonf_duplicate_entry_add(
    struct oonf_duplicate_set *, uint8_t msg_type,
    struct netaddr *, uint16_t seqno, uint64_t vtime);

EXPORT enum oonf_duplicate_result oonf_duplicate_test(
    struct oonf_duplicate_set *, uint8_t msg_type,
    struct netaddr *, uint16_t seqno);

static INLINE bool
oonf_duplicate_is_new(enum oonf_duplicate_result result) {
  return result == OONF_DUPSET_NEW || result == OONF_DUPSET_NEWEST;
}

#endif /* OONF_DUPLICATE_SET_H_ */

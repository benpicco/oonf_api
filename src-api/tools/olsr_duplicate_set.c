
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

#include "common/avl.h"
#include "common/common_types.h"
#include "common/netaddr.h"
#include "rfc5444/rfc5444.h"
#include "core/olsr_class.h"
#include "core/olsr_timer.h"
#include "tools/olsr_duplicate_set.h"

void _cb_vtime(void *);
enum olsr_duplicate_result _test(struct olsr_duplicate_entry *,
    uint16_t seqno, bool set);

static struct olsr_timer_info _vtime_info = {
  .name = "Valdity time for duplicate set",
  .callback = _cb_vtime,
};

static struct olsr_class _vtime_class = {
  .name = "Duplicate set",
  .size = sizeof(struct olsr_duplicate_entry),
};

/**
 * Initialize duplicate set subsystem
 */
void
olsr_duplicate_set_init(void) {
  olsr_class_add(&_vtime_class);
  olsr_timer_add(&_vtime_info);
}

/**
 * Cleanup duplicate set subsystem
 */
void
olsr_duplicate_set_cleanup(void) {
  olsr_timer_remove(&_vtime_info);
  olsr_class_remove(&_vtime_class);
}

/**
 * Test a originator/seqno pair against a duplicate set and add
 * it to the set if necessary
 * @param set duplicate set
 * @param originator originator of sequence number
 * @param seqno sequence number
 * @param vtime validity time of sequence number
 * @return OLSR_DUPSET_TOO_OLD if sequence number is more than 32 behind
 *   the current one, OLSR_DUPSET_DUPLICATE if the number is in the set,
 *   OLSR_DUPSET_NEW if the number was added to the set and OLSR_DUPSET_NEWEST
 *   if the sequence number is newer than the newest in the set
 */
enum olsr_duplicate_result
olsr_duplicate_entry_add(struct olsr_duplicate_set *set,
    struct netaddr *originator, uint16_t seqno, uint64_t vtime) {
  struct olsr_duplicate_entry *entry;

  entry = avl_find_element(&set->_tree, originator, entry, _node);
  if (!entry) {
    entry = olsr_class_malloc(&_vtime_class);
    if (entry == NULL) {
      return OLSR_DUPSET_TOO_OLD;
    }

    /* initialize history and current sequence number */
    entry->current = seqno;
    entry->history = 1;

    /* initialize backpointer */
    entry->set = set;

    /* initialize vtime */
    entry->_vtime.info = &_vtime_info;
    entry->_vtime.cb_context = entry;

    olsr_timer_start(&entry->_vtime, vtime);

    /* set key and link entry to set */
    memcpy(&entry->addr, originator, sizeof(*originator));
    entry->_node.key = &entry->addr;
    avl_insert(&set->_tree, &entry->_node);

    return OLSR_DUPSET_NEWEST;
  }

  return _test(entry, seqno, true);
}

/**
 * Test a originator/sequence number pair against a duplicate set
 * @param set duplicate set
 * @param originator originator of sequence number
 * @param seqno sequence number
 * @return OLSR_DUPSET_TOO_OLD if sequence number is more than 32 behind
 *   the current one, OLSR_DUPSET_DUPLICATE if the number is in the set,
 *   OLSR_DUPSET_NEW if the number was added to the set and OLSR_DUPSET_NEWEST
 *   if the sequence number is newer than the newest in the set
 */
enum olsr_duplicate_result
olsr_duplicate_test(struct olsr_duplicate_set *set,
    struct netaddr *originator, uint16_t seqno) {
  struct olsr_duplicate_entry *entry;

  entry = avl_find_element(&set->_tree, originator, entry, _node);
  if (!entry) {
    return OLSR_DUPSET_NEWEST;
  }

  return _test(entry, seqno, false);
}

/**
 * Test a sequence number against a duplicate set entry
 * @param entry duplicate set entry
 * @param seqno sequence number
 * @param set true to add the sequence number to the entry, false
 *   to leave the entry unchanged.
 * @return OLSR_DUPSET_TOO_OLD if sequence number is more than 32 behind
 *   the current one, OLSR_DUPSET_DUPLICATE if the number is in the set,
 *   OLSR_DUPSET_NEW if the number was added to the set and OLSR_DUPSET_NEWEST
 *   if the sequence number is newer than the newest in the set
 */
enum olsr_duplicate_result
_test(struct olsr_duplicate_entry *entry,
    uint16_t seqno, bool set) {
  int current;

  if (seqno == entry->current) {
    return OLSR_DUPSET_DUPLICATE;
  }

  /* eliminate rollover */
  current = entry->current;
  if (rfc5444_seqno_is_larger(current, seqno) && current < seqno) {
    /* eliminate rollover */
    current += 65536;
  }
  else if (rfc5444_seqno_is_larger(seqno, current) && seqno < current) {
    current -= 65536;
  }

  if (current > seqno + 31) {
    return OLSR_DUPSET_TOO_OLD;
  }

  if (current > seqno) {
    if ((entry->history & (1 << (current - seqno))) != 0) {
      return OLSR_DUPSET_DUPLICATE;
    }
    return OLSR_DUPSET_NEW;
  }

  if (set) {
    /* new sequence number is larger than last one */
    entry->current = seqno;

    if (seqno > current + 31) {
      entry->history = 1;
    }
    else {
      entry->history <<= (seqno - current - 1);
      entry->history |= 1;
    }
  }
  return OLSR_DUPSET_NEWEST;
}

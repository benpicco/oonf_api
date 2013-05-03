
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

#ifndef OLSR_TIMER_H_
#define OLSR_TIMER_H_

#include "common/common_types.h"
#include "common/list.h"
#include "common/avl.h"

#include "core/olsr_clock.h"

/* prototype for timer callback */
typedef void (*timer_cb_func) (void *);

struct olsr_timer_entry;

/*
 * This struct defines a class of timers which have the same
 * type (periodic/non-periodic) and callback.
 */
struct olsr_timer_info {
  /* _node of timerinfo list */
  struct list_entity _node;

  /* name of this timer class */
  const char *name;

  /* callback function */
  timer_cb_func callback;

  /* true if this is a class of periodic timers */
  bool periodic;

  /* Stats, resource usage */
  uint32_t usage;

  /* Stats, resource churn */
  uint32_t changes;

  /* pointer to timer currently in callback */
  struct olsr_timer_entry *_timer_in_callback;

  /* set to true if the current running timer has been stopped */
  bool _timer_stopped;
};

/*
 * Our timer implementation is a based on individual timers arranged in
 * a double linked list hanging in a hierarchical list of timer slots.
 *
 * When an event is triggered, its callback is called with cb_context
 * as its parameter.
 */
struct olsr_timer_entry {
  /* Tree membership */
  struct avl_node _node;

  /* backpointer to timer info */
  struct olsr_timer_info *info;

  /* the jitter expressed in percent */
  uint8_t jitter_pct;

  /* context pointer */
  void *cb_context;

  /* timeperiod between two timer events for periodical timers */
  uint64_t _period;

  /* cache random() result for performance reasons */
  unsigned int _random;

  /* absolute timestamp when timer will fire */
  uint64_t _clock;
};

EXPORT extern struct oonf_subsystem oonf_timer_subsystem;

/* Timers */
EXPORT extern struct list_entity timerinfo_list;
#define OLSR_FOR_ALL_TIMERS(ti, iterator) list_for_each_element_safe(&timerinfo_list, ti, _node, iterator)

EXPORT void olsr_timer_walk(void);

EXPORT void olsr_timer_add(struct olsr_timer_info *ti);
EXPORT void olsr_timer_remove(struct olsr_timer_info *);

EXPORT void olsr_timer_set(struct olsr_timer_entry *timer, uint64_t rel_time);
EXPORT void olsr_timer_start(struct olsr_timer_entry *timer, uint64_t rel_time);
EXPORT void olsr_timer_stop(struct olsr_timer_entry *);

EXPORT uint64_t olsr_timer_getNextEvent(void);

/**
 * @param timer pointer to timer
 * @return true if the timer is running, false otherwise
 */
static INLINE bool
olsr_timer_is_active(const struct olsr_timer_entry *timer) {
  return timer->_clock != 0ull;
}

/**
 * @param timer pointer to timer
 * @return interval between timer events in milliseconds
 */
static INLINE uint64_t
olsr_timer_get_period(const struct olsr_timer_entry *timer) {
  return timer->_period;
}

/**
 * @param timer pointer to timer
 * @return number of milliseconds until timer fires
 */
static INLINE int64_t
olsr_timer_get_due(const struct olsr_timer_entry *timer) {
  return olsr_clock_get_relative(timer->_clock);
}


#endif /* OLSR_TIMER_H_ */

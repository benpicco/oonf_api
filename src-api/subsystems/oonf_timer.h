
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

#ifndef OONF_TIMER_H_
#define OONF_TIMER_H_

#include "common/common_types.h"
#include "common/list.h"
#include "common/avl.h"

#include "subsystems/oonf_clock.h"

/* prototype for timer callback */
typedef void (*timer_cb_func) (void *);

struct oonf_timer_entry;

/*
 * This struct defines a class of timers which have the same
 * type (periodic/non-periodic) and callback.
 */
struct oonf_timer_info {
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
  struct oonf_timer_entry *_timer_in_callback;

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
struct oonf_timer_entry {
  /* Tree membership */
  struct avl_node _node;

  /* backpointer to timer info */
  struct oonf_timer_info *info;

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

#define LOG_TIMER oonf_timer_subsystem.logging
EXPORT extern struct oonf_subsystem oonf_timer_subsystem;

/* Timers */
EXPORT extern struct list_entity oonf_timer_info_list;

EXPORT void oonf_timer_walk(void);

EXPORT void oonf_timer_add(struct oonf_timer_info *ti);
EXPORT void oonf_timer_remove(struct oonf_timer_info *);

EXPORT void oonf_timer_set_ext(struct oonf_timer_entry *timer, uint64_t first, uint64_t interval);
EXPORT void oonf_timer_start_ext(struct oonf_timer_entry *timer, uint64_t first, uint64_t interval);
EXPORT void oonf_timer_stop(struct oonf_timer_entry *);

EXPORT uint64_t oonf_timer_getNextEvent(void);

/**
 * @param timer pointer to timer
 * @return true if the timer is running, false otherwise
 */
static INLINE bool
oonf_timer_is_active(const struct oonf_timer_entry *timer) {
  return timer->_clock != 0ull;
}

/**
 * @param timer pointer to timer
 * @return interval between timer events in milliseconds
 */
static INLINE uint64_t
oonf_timer_get_period(const struct oonf_timer_entry *timer) {
  return timer->_period;
}

/**
 * @param timer pointer to timer
 * @return number of milliseconds until timer fires
 */
static INLINE int64_t
oonf_timer_get_due(const struct oonf_timer_entry *timer) {
  return oonf_clock_get_relative(timer->_clock);
}

/**
 * This is the one stop shop for all sort of timer manipulation.
 * Depending on the passed in parameters a new timer is started,
 * or an existing timer is started or an existing timer is
 * terminated.
 * @param timer timer_entry pointer
 * @param rel_time relative time when the timer should fire
 */
static INLINE void
oonf_timer_set(struct oonf_timer_entry *timer, uint64_t rel_time) {
  oonf_timer_set_ext(timer, rel_time, rel_time);
}

/**
 * Start or restart a new timer.
 * @param timer initialized timer entry
 * @param rel_time relative time when the timer should fire
 */
static INLINE void
oonf_timer_start(struct oonf_timer_entry *timer, uint64_t rel_time) {
  oonf_timer_start_ext(timer, rel_time, rel_time);
}

#endif /* OONF_TIMER_H_ */

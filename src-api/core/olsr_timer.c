
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
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/avl.h"
#include "common/common_types.h"
#include "core/olsr_clock.h"
#include "core/olsr_logging.h"
#include "core/olsr_class.h"
#include "core/olsr_timer.h"
#include "core/olsr_subsystem.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static void _calc_clock(struct olsr_timer_entry *timer, uint64_t rel_time);
static int _avlcomp_timer(const void *p1, const void *p2);

/* minimal granularity of the timer system in milliseconds */
const uint64_t TIMESLICE = 100;

/* tree of all timers */
static struct avl_tree _timer_tree;

/* true if scheduler is active */
static bool _scheduling_now;

/* List of timer classes */
struct list_entity timerinfo_list;

/* subsystem definition */
struct oonf_subsystem oonf_timer_subsystem = {
  .init = _init,
  .cleanup = _cleanup,
};

/**
 * Initialize timer scheduler subsystem
 * @return always returns 0
 */
int
_init(void)
{
  OLSR_INFO(LOG_TIMER, "Initializing timer scheduler.\n");

  avl_init(&_timer_tree, _avlcomp_timer, true);
  _scheduling_now = false;

  list_init_head(&timerinfo_list);
  return 0;
}

/**
 * Cleanup timer scheduler, this stops and deletes all timers
 */
static void
_cleanup(void)
{
  struct olsr_timer_info *ti, *iterator;

  /* free all timerinfos */
  OLSR_FOR_ALL_TIMERS(ti, iterator) {
    olsr_timer_remove(ti);
  }
}

/**
 * Add a new group of timers to the scheduler
 * @param ti pointer to uninitialized timer info
 */
void
olsr_timer_add(struct olsr_timer_info *ti) {
  assert (ti->callback);
  assert (ti->name);
  list_add_tail(&timerinfo_list, &ti->_node);
}

/**
 * Removes a group of timers from the scheduler
 * All pointers to timers of this info will be invalid after this.
 * @param info pointer to timer info
 */
void
olsr_timer_remove(struct olsr_timer_info *info) {
  struct olsr_timer_entry *timer, *iterator;

  avl_for_each_element_safe(&_timer_tree, timer, _node, iterator) {
    if (timer->info == info) {
      olsr_timer_stop(timer);
    }
  }

  list_remove(&info->_node);
}

/**
 * Start or restart a new timer.
 * @param timer initialized timer entry
 * @param rel_time relative time when the timer should fire
 */
void
olsr_timer_start(struct olsr_timer_entry *timer, uint64_t rel_time)
{
#if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_DEBUG
  struct fraction_str timebuf1;
#endif

  assert(timer->info);
  assert(timer->jitter_pct <= 100);

  if (timer->_clock) {
    avl_remove(&_timer_tree, &timer->_node);
  }
  else {
    timer->_node.key = timer;
    timer->info->usage++;
  }
  timer->info->changes++;

  /*
   * Compute random numbers only once.
   */
  if (!timer->_random) {
    timer->_random = random();
  }

  /* Fill entry */
  _calc_clock(timer, rel_time);

  /* Singleshot or periodical timer ? */
  timer->_period = timer->info->periodic ? rel_time : 0;

  /* insert into tree */
  avl_insert(&_timer_tree, &timer->_node);

  OLSR_DEBUG(LOG_TIMER, "TIMER: start timer '%s' firing in %s (%"PRIu64")\n",
      timer->info->name,
      olsr_clock_toClockString(&timebuf1, rel_time), timer->_clock);

  /* fix 'next event' pointers if necessary */
  if (_scheduling_now) {
    /* will be fixed at the end of the timer scheduling loop */
    return;
  }
}

/**
 * Delete a timer.
 * @param timer the olsr_timer_entry that shall be removed
 */
void
olsr_timer_stop(struct olsr_timer_entry *timer)
{
  if (timer->_clock == 0) {
    return;
  }

  OLSR_DEBUG(LOG_TIMER, "TIMER: stop %s\n", timer->info->name);

  /* remove timer from tree */
  avl_remove(&_timer_tree, &timer->_node);
  timer->_clock = 0;
  timer->_random = 0;
  timer->info->usage--;
  timer->info->changes++;

  if (timer->info->_timer_in_callback == timer) {
    timer->info->_timer_stopped = true;
  }
}

/**
 * This is the one stop shop for all sort of timer manipulation.
 * Depending on the passed in parameters a new timer is started,
 * or an existing timer is started or an existing timer is
 * terminated.
 * @param timer timer_entry pointer
 * @param rel_time time until the new timer should fire, 0 to stop timer
 */
void
olsr_timer_set(struct olsr_timer_entry *timer, uint64_t rel_time)
{
  if (rel_time == 0) {
    /* No good future time provided, kill it. */
    olsr_timer_stop(timer);
  }
  else {
    /* Start or restart the timer */
    olsr_timer_start(timer, rel_time);
  }
}

/**
 * Walk through the timer list and check if any timer is ready to fire.
 * Call the provided function with the context pointer.
 */
void
olsr_timer_walk(void)
{
  struct olsr_timer_entry *timer;
  struct olsr_timer_info *info;

  _scheduling_now = true;

  while (true) {
    timer = avl_first_element(&_timer_tree, timer, _node);

    if (timer->_clock > olsr_clock_getNow()) {
      break;
    }

    OLSR_DEBUG(LOG_TIMER, "TIMER: fire '%s' at clocktick %" PRIu64 "\n",
                  timer->info->name, timer->_clock);

    /*
     * The timer->info pointer is invalidated by olsr_timer_stop()
     */
    info = timer->info;
    info->_timer_in_callback = timer;
    info->_timer_stopped = false;

    /* update statistics */
    info->changes++;

    if (timer->_period == 0) {
      /* stop now, the data structure might not be available anymore later */
      olsr_timer_stop(timer);
    }

    /* This timer is expired, call into the provided callback function */
    timer->info->callback(timer->cb_context);

    /*
     * Only act on actually running timers, the callback might have
     * called olsr_timer_stop() !
     */
    if (!info->_timer_stopped) {
      /*
       * Timer has been not been stopped, so its periodic.
       * rehash the random number and restart.
       */
      timer->_random = random();
      olsr_timer_start(timer, timer->_period);
    }
  }

  _scheduling_now = false;
}

/**
 * @return timestamp when next timer will fire
 */
uint64_t
olsr_timer_getNextEvent(void) {
  struct olsr_timer_entry *first;

  if (avl_is_empty(&_timer_tree)) {
    return UINT64_MAX;
  }

  first = avl_first_element(&_timer_tree, first, _node);
  return first->_clock;
}

/**
 * Decrement a relative timer by a random number range.
 * @param the relative timer expressed in units of milliseconds.
 * @param the jitter in percent
 * @param random_val cached random variable to calculate jitter
 * @return the absolute time when timer will fire
 */
static void
_calc_clock(struct olsr_timer_entry *timer, uint64_t rel_time)
{
  uint64_t t = 0;
  unsigned random_jitter;

  if (timer->jitter_pct) {
    /*
     * Play some tricks to avoid overflows with integer arithmetic.
     */
    random_jitter = timer->_random / (RAND_MAX / timer->jitter_pct);
    t = (uint64_t)random_jitter * rel_time / 100;

    OLSR_DEBUG(LOG_TIMER, "TIMER: jitter %u%% rel_time %" PRIu64 "ms to %" PRIu64 "ms\n",
        timer->jitter_pct, rel_time, rel_time - t);

    rel_time -= t;
  }

  timer->_clock = olsr_clock_get_absolute(rel_time - t);

  /* round up to next timeslice */
  timer->_clock += TIMESLICE;
  timer->_clock -= (timer->_clock % TIMESLICE);
}

/**
 * Custom AVL comparator for two timer entries.
 * @param p1
 * @param p2
 * @return
 */
static int
_avlcomp_timer(const void *p1, const void *p2) {
  const struct olsr_timer_entry *t1, *t2;

  t1 = p1;
  t2 = p2;

  if (t1->_clock > t2->_clock) {
    return 1;
  }
  if (t1->_clock < t2->_clock) {
    return -1;
  }
  return 0;
}

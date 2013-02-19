
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

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "common/avl.h"
#include "common/common_types.h"
#include "core/olsr_clock.h"
#include "core/olsr_logging.h"
#include "core/olsr_object.h"
#include "core/olsr_timer.h"
#include "core/olsr_subsystem.h"

/* Number of hierarchies of buckets */
#define BUCKET_DEPTH          3

/* power of 2 for the number of elements in each bucket */
#define BUCKET_COUNT_POW2     9ull

/* power of 2 for the number of milliseconds each bucket represents */
#define BUCKET_TIMESLICE_POW2 7ull

/* maximum number of milliseconds a timer can use */
const uint64_t BUCKET_COUNT = (1ull << BUCKET_COUNT_POW2);
const uint64_t BUCKET_TIMESLICE = (1ull << BUCKET_TIMESLICE_POW2);
const uint64_t TIMER_MAX_RELTIME = (1ull << (BUCKET_COUNT_POW2 * BUCKET_DEPTH + BUCKET_TIMESLICE_POW2));
const uint64_t BUCKET_TIMESLICE_MASK = ~((1ull << BUCKET_TIMESLICE_POW2) - 1);

/* root of all timers */
static struct list_entity _buckets[1ull << BUCKET_COUNT_POW2][BUCKET_DEPTH];
static uint32_t _bucket_ptr[BUCKET_DEPTH];

/* time when the next timer will fire */
static uint64_t _next_event;

/* false if the _next_valid variable must be recalculated */
static bool _next_event_valid;

/* number of timer events still in queue */
static uint32_t _total_timer_events;

/* true if scheduler is active */
static bool _scheduling_now;

static struct list_entity *_scheduler_next_ptr;

/* Memory cookie for the timer manager */
struct list_entity timerinfo_list;

/* remember if initialized or not */
OLSR_SUBSYSTEM_STATE(_timer_state);

/* Prototypes */
static void _calc_clock(struct olsr_timer_entry *timer, uint64_t rel_time);
static int _insert_into_bucket(struct olsr_timer_entry *);
static void _calculate_next_event(void);

/**
 * Initialize timer scheduler subsystem
 * @return -1 if an error happened, 0 otherwise
 */
void
olsr_timer_init(void)
{
  uint64_t now, offset;
  unsigned i,j;

  if (olsr_subsystem_init(&_timer_state))
    return;

  OLSR_INFO(LOG_TIMER, "Initializing timer scheduler.\n");
  /* mask "last run" to slot size */
  now = olsr_clock_getNow();

  offset = BUCKET_TIMESLICE * BUCKET_COUNT;
  now >>= BUCKET_TIMESLICE_POW2;

  for (i = 0; i < BUCKET_DEPTH; i++) {
    _bucket_ptr[i] = now & (BUCKET_COUNT - 1ull);

    now >>= BUCKET_COUNT_POW2;
    offset <<= BUCKET_COUNT_POW2;

    for (j = 0; j < BUCKET_COUNT; j++) {
      list_init_head(&_buckets[j][i]);
    }
  }

  /* at the moment we have no timer */
  _next_event = ~0ull;
  _total_timer_events = 0;
  _scheduling_now = false;
  _scheduler_next_ptr = NULL;
  _next_event_valid = true;

  list_init_head(&timerinfo_list);
}

/**
 * Cleanup timer scheduler, this stops and deletes all timers
 */
void
olsr_timer_cleanup(void)
{
  struct olsr_timer_info *ti, *iterator;
  struct list_entity *timer_head_node;
  unsigned i,j;

  if (olsr_subsystem_cleanup(&_timer_state))
    return;

  for (i = 0; i < BUCKET_DEPTH; i++) {
    for (j = 0; j < BUCKET_COUNT; j++) {
      timer_head_node = &_buckets[j][i];

      /* Kill all entries hanging off this hash bucket. */
      while (!list_is_empty(timer_head_node)) {
        struct olsr_timer_entry *timer;

        timer = list_first_element(timer_head_node, timer, _node);
        olsr_timer_stop(timer);
      }
    }
  }

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
  unsigned i,j;

  for (i = 0; i < BUCKET_DEPTH; i++) {
    for (j = 0; j < BUCKET_COUNT; j++) {
      list_for_each_element_safe(&_buckets[j][i], timer, _node, iterator) {
        /* remove all timers of this info */
        if (timer->info == info) {
          olsr_timer_stop(timer);
        }
      }
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
  int new_slot;

  assert(timer->info);
  assert(timer->jitter_pct <= 100);
  assert(rel_time > 0 && rel_time < TIMER_MAX_RELTIME);

  if (timer->_clock) {
    /* move scheduler next ptr if necessary */
    if (&timer->_node == _scheduler_next_ptr) {
      _scheduler_next_ptr = _scheduler_next_ptr->next;
    }

    list_remove(&timer->_node);
    _total_timer_events--;
  }
  else {
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

  /*
   * Now insert in the respective _timer_wheel slot.
   */
  new_slot = _insert_into_bucket(timer);

  /* and update internal time data */
  _total_timer_events++;

  OLSR_DEBUG(LOG_TIMER, "TIMER: start timer '%s' firing in %s (%"PRIu64")\n",
      timer->info->name,
      olsr_clock_toClockString(&timebuf1, rel_time), timer->_clock);

  /* fix 'next event' pointers if necessary */
  if (_scheduling_now) {
    /* will be fixed at the end of the timer scheduling loop */
    return;
  }

  if (timer->_clock <= _next_event) {
    /* we have a new 'earliest' event */
    OLSR_DEBUG(LOG_TIMER, "Next event set to %"PRIu64" (timer_start", _next_event);
    _next_event = timer->_clock;

    if (new_slot != -1) {
      /* earliest event in depth 0 buckets ! */
      _bucket_ptr[0] = new_slot;
    }
    else {
      /* just jump to the last bucket of the current loop to speed up scheduler */
      _bucket_ptr[0] = BUCKET_COUNT - 1;
    }
  }
  else if (list_is_empty(&_buckets[_bucket_ptr[0]][0])) {
    /* event got later and now the 'earliest bucket' is empty */
    _next_event_valid =  false;
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

  /* move scheduler next ptr if necessary */
  if (&timer->_node == _scheduler_next_ptr) {
    _scheduler_next_ptr = _scheduler_next_ptr->next;
  }

  /* remove timer from buckets */
  list_remove(&timer->_node);
  timer->_clock = 0;
  timer->_random = 0;
  timer->info->usage--;
  timer->info->changes++;

  if (timer->info->_timer_in_callback == timer) {
    timer->info->_timer_stopped = true;
  }

  /* and update internal time data */
  _total_timer_events--;
  if (!_scheduling_now && list_is_empty(&_buckets[_bucket_ptr[0]][0])) {
    /* we are outside the event loop and now the 'earliest bucket' is empty */
    _next_event_valid = false;
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

  int i;

  _scheduling_now = true;

  while (_next_event <= olsr_clock_getNow()) {
    i = _bucket_ptr[0];

    _scheduler_next_ptr = _buckets[i][0].next;
    while (_scheduler_next_ptr != &_buckets[i][0]) {
      timer = container_of(_scheduler_next_ptr, struct olsr_timer_entry, _node);
      _scheduler_next_ptr = _scheduler_next_ptr->next;

      OLSR_DEBUG(LOG_TIMER, "TIMER: fire '%s' at clocktick %" PRIu64 "\n",
                  timer->info->name, _next_event);

      /* get pointer to next timer */

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
         * Timer has been not been stopped, so its periodic
         * rehash the random number and restart
         */
        timer->_random = random();
        olsr_timer_start(timer, timer->_period);
      }
    }

    /* advance our 'next event' marker */
    _calculate_next_event();
  }

  _scheduling_now = false;
}

/**
 * @return timestamp when next timer will fire
 */
uint64_t
olsr_timer_getNextEvent(void) {
  if (!_next_event_valid) {
    _calculate_next_event();
  }
  return _next_event;
}

/**
 * Puts a timer into the corresponding bucket
 * @param timer pointer to initialized timer struct
 * @return index of bucket if in group 0, otherwise -1
 */
static int
_insert_into_bucket(struct olsr_timer_entry *timer) {
  uint64_t slot;
  int64_t relative;
  int group;

  slot = timer->_clock >> BUCKET_TIMESLICE_POW2;
  relative = olsr_clock_get_relative(timer->_clock) >> BUCKET_TIMESLICE_POW2;

  if (relative == 0) {
    /* do not put new timer into current bucket ! */
    relative = 1;
    slot++;
  }

  for (group = 0; group < BUCKET_DEPTH;
      group++, slot >>= BUCKET_COUNT_POW2, relative >>= BUCKET_COUNT_POW2) {
    if (relative < (int64_t)BUCKET_COUNT) {
      slot &= (BUCKET_COUNT - 1);

      list_add_tail(&_buckets[slot][group], &timer->_node);

      OLSR_DEBUG(LOG_TIMER, "Put timer %s (%"PRIu64") into bucket (%"PRIu64"/%d)",
          timer->info->name, timer->_clock, slot, group);
      return group == 0 ? (int)slot : -1;
    }
  }

  OLSR_WARN(LOG_TIMER, "Error, timer event too far in the future: %" PRIu64,
      olsr_clock_get_relative(timer->_clock));
  return -1;
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
  }
  t = olsr_clock_get_absolute(rel_time - t);

  /* round to bucket border */
  timer->_clock = (t + BUCKET_TIMESLICE/2) & BUCKET_TIMESLICE_MASK;
}

static void
_copy_bucket(unsigned depth, unsigned idx) {
  struct olsr_timer_entry *timer, *timer_it;
  uint64_t shift;
  unsigned i;

  assert (depth > 0 && depth < BUCKET_DEPTH && idx < BUCKET_COUNT);

  OLSR_DEBUG(LOG_TIMER, "Copy bucket %u depth %u to depth %u",
      idx, depth, depth-1);

  shift = BUCKET_TIMESLICE_POW2 + BUCKET_COUNT_POW2*depth;
  _bucket_ptr[depth] = idx+1;

  list_for_each_element_safe(&_buckets[idx][depth], timer, _node, timer_it) {
    list_remove(&timer->_node);

    i = (timer->_clock >> shift) & (BUCKET_COUNT - 1ull);
    list_add_tail(&_buckets[i][depth-1], &timer->_node);
  }
}

static int
_look_for_event(unsigned depth) {
  unsigned i;
  int idx;
  bool overflow;
  OLSR_DEBUG(LOG_TIMER, "Look for event depth %u, starting at bucket %d",
      depth, _bucket_ptr[depth]);

  /* first look in existing data before we need to load another layer */
  for (i=_bucket_ptr[depth]; i < BUCKET_COUNT; i++) {
    if (!list_is_empty(&_buckets[i][depth])) {
      OLSR_DEBUG(LOG_TIMER, "\tFound something in bucket %d", i);
      return i;
    }
  }

  /* now look if the current depth contains data in the 'overflow' area */
  overflow = false;
  for (i = 0; i<_bucket_ptr[depth]; i++) {
    if (!list_is_empty(&_buckets[i][depth])) {
      overflow = true;
      break;
    }
  }

  /* copy bucket from level higher if possible */
  if (depth < BUCKET_DEPTH - 1) {
    if (overflow) {
      idx = _bucket_ptr[depth + 1];
    }
    else {
      idx = _look_for_event(depth+1);
    }

    if (idx != -1) {
      _copy_bucket(depth+1, idx);
    }
  }

  /* now look again for a full bucket */
  for (i=0; i < BUCKET_COUNT; i++) {
    if (!list_is_empty(&_buckets[i][depth])) {
      OLSR_DEBUG(LOG_TIMER, "\tFound something in bucket %d", i);
      return i;
    }
  }

  return -1;
}

static void
_calculate_next_event(void) {
  struct olsr_timer_entry *timer;
  int idx;

  /* prevent multiple recalculations */
  _next_event_valid = true;

  /* no timer event in queue ? */
  if (_total_timer_events == 0) {
    _next_event = ~0ull;
    return;
  }

  idx = _look_for_event(0);
  assert (idx >= 0);

  _bucket_ptr[0] = idx;

  /* get the timestamp when the first bucket will happen */
  timer = list_first_element(&_buckets[_bucket_ptr[0]][0], timer, _node);

  OLSR_DEBUG(LOG_TIMER, "Next event moved from %"PRIu64" to %"PRIu64,
      _next_event, timer->_clock);
  _next_event = timer->_clock;
}

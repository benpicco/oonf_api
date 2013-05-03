
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
#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>

#include "config/cfg_schema.h"
#include "config/cfg.h"

#include "core/os_clock.h"
#include "core/olsr_logging.h"
#include "core/olsr_clock.h"
#include "core/olsr_subsystem.h"

/* prototypes */
static int _init(void);

/* absolute monotonic clock measured in milliseconds compared to start time */
static uint64_t now_times;

/* arbitrary timestamp that represents the time olsr_clock_init() was called */
static uint64_t start_time;

struct oonf_subsystem oonf_clock_subsystem = {
  .init = _init,
};

/**
 * Initialize olsr clock system
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  if (os_clock_gettime64(&start_time)) {
    OLSR_WARN(LOG_TIMER, "OS clock is not working: %s (%d)\n", strerror(errno), errno);
    return -1;
  }

  now_times = 0;

  return 0;
}

/**
 * Update the internal clock to current system time
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_clock_update(void)
{
  uint64_t now;
  if (os_clock_gettime64(&now)) {
    OLSR_WARN(LOG_TIMER, "OS clock is not working: %s (%d)\n", strerror(errno), errno);
    return -1;
  }

  now_times = now - start_time;
  return 0;
}

/**
 * Calculates the current time in the internal OLSR representation
 * @return current time
 */
uint64_t
olsr_clock_getNow(void) {
  return now_times;
}

/**
 * Format an internal time value into a string.
 * Displays hours:minutes:seconds.millisecond.
 *
 * @param buf string buffer for creating output
 * @param clk OLSR timestamp
 * @return buffer to a formatted system time string.
 */
const char *
olsr_clock_toClockString(struct fraction_str *buf, uint64_t clk)
{
  uint64_t msec = clk % MSEC_PER_SEC;
  uint64_t sec = clk / MSEC_PER_SEC;

  snprintf(buf->buf, sizeof(*buf),
      "%"PRIu64":%02"PRIu64":%02"PRIu64".%03"PRIu64"",
      sec / 3600, (sec % 3600) / 60, (sec % 60), msec);

  return buf->buf;
}


/**
 * Checks a value of a CLOCK field for validity.
 * See CFG_VALIDATE_TIME() macro in olr_clock.h
 * @param entry pointer to schema entry
 * @param section_name name of section type and name
 * @param value value of schema entry
 * @param out pointer to autobuffer for validator output
 * @return 0 if validation found no problems, -1 otherwise
 */
int
olsr_clock_validate(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  uint64_t num;

  if (olsr_clock_fromIntervalString(&num, value)) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s is not a valid fractional integer"
        " (positive or zero, maximum of 3 fractional digits)",
        value, entry->key.entry, section_name);
    return -1;
  }

  if (num < entry->validate_param[0].u64) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s must be larger than %"PRIu64")",
        value, entry->key.entry, section_name,
        entry->validate_param[0].u64);
    return -1;
  }
  if (num > entry->validate_param[1].u64) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s must be smaller than %"PRIu64"",
        value, entry->key.entry, section_name,
        entry->validate_param[1].u64);
    return -1;
  }
  return 0;
}

/**
 * Binary converter for time intervals.
 * See CFG_MAP_TIME() macro in olr_clock.h
 * @param s_entry pointer to configuration entry schema.
 * @param value pointer to value of configuration entry.
 * @param reference pointer to binary output buffer.
 * @return 0 if conversion succeeded, -1 otherwise.
 */
int
olsr_clock_tobin(
    const struct cfg_schema_entry *s_entry __attribute__((unused)),
    const struct const_strarray *value, void *reference) {
  return olsr_clock_fromIntervalString(reference, strarray_get_first_c(value));
}

/**
 * Help generator for time validator.
 * See CFG_VALIDATE_TIME() and CFG_VALIDATE_TIME_MINMAX() macro in olr_clock.h
 * @param entry pointer to schema entry
 * @param out pointer to autobuffer for validator output
 */
void
olsr_clock_help(
    const struct cfg_schema_entry *entry, struct autobuf *out) {
  struct fraction_str buf;

  cfg_append_printable_line(out,
      "    Parameter must be an timestamp with a maximum of 3 fractional digits");
  if (entry->validate_param[0].u64 != 0) {
    cfg_append_printable_line(out, "    Minimal valid time is %s",
        olsr_clock_toClockString(&buf, entry->validate_param[0].u64));
  }
  if (entry->validate_param[1].u64 != UINT64_MAX) {
    cfg_append_printable_line(out, "    Maximum valid time is %s",
        olsr_clock_toClockString(&buf, entry->validate_param[1].u64));
  }
}

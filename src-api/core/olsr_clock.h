
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

#ifndef _OLSR_CLOCK
#define _OLSR_CLOCK

#include "common/common_types.h"
#include "config/cfg.h"
#include "config/cfg_schema.h"

/* Some defs for juggling with timers */
#define MSEC_PER_SEC 1000
#define USEC_PER_SEC 1000000
#define NSEC_PER_USEC 1000
#define USEC_PER_MSEC 1000

/* definitions for config parser usage */
#define CFG_VALIDATE_CLOCK(p_name, p_def, p_help, args...)                      _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_clock_validate, .cb_valhelp = olsr_clock_help, .validate_param = {{.u64 = 0}, {.u64 = UINT64_MAX}}, ##args )
#define CFG_VALIDATE_CLOCK_MIN(p_name, p_def, p_help, p_min, args...)           _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_clock_validate, .cb_valhelp = olsr_clock_help, .validate_param = {{.u64 = p_min}, {.u64 = UINT64_MAX}},##args )
#define CFG_VALIDATE_CLOCK_MAX(p_name, p_def, p_help, p_min, args...)           _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_clock_validate, .cb_valhelp = olsr_clock_help, .validate_param = {{.u64 = 0}, {.u64 = p_max}},##args )
#define CFG_VALIDATE_CLOCK_MINMAX(p_name, p_def, p_help, p_min, p_max, args...) _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_clock_validate, .cb_valhelp = olsr_clock_help, .validate_param = {{.u64 = p_min}, {.u64 = p_max}},##args )

#define CFG_MAP_CLOCK(p_reference, p_field, p_name, p_def, p_help, args...)                      CFG_VALIDATE_CLOCK(p_name, p_def, p_help, .cb_to_binary = olsr_clock_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)
#define CFG_MAP_CLOCK_MIN(p_reference, p_field, p_name, p_def, p_help, p_min, args...)           CFG_VALIDATE_CLOCK_MIN(p_name, p_def, p_help, p_min, .cb_to_binary = olsr_clock_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)
#define CFG_MAP_CLOCK_MAX(p_reference, p_field, p_name, p_def, p_help, p_max, args...)           CFG_VALIDATE_CLOCK_MAX(p_name, p_def, p_help, p_max, .cb_to_binary = olsr_clock_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)
#define CFG_MAP_CLOCK_MINMAX(p_reference, p_field, p_name, p_def, p_help, p_min, p_max, args...) CFG_VALIDATE_CLOCK_MINMAX(p_name, p_def, p_help, p_min, p_max, .cb_to_binary = olsr_clock_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)

EXPORT int olsr_clock_init(void) __attribute__((warn_unused_result));
EXPORT void olsr_clock_cleanup(void);
EXPORT int olsr_clock_update(void) __attribute__((warn_unused_result));

EXPORT uint64_t olsr_clock_getNow(void);

EXPORT const char *olsr_clock_toClockString(struct fraction_str *, uint64_t);

EXPORT int olsr_clock_validate(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out);
EXPORT int olsr_clock_tobin(const struct cfg_schema_entry *s_entry,
    const struct const_strarray *value, void *reference);
EXPORT void  olsr_clock_help(
    const struct cfg_schema_entry *entry, struct autobuf *out);

static INLINE const char *
olsr_clock_toIntervalString(struct fraction_str *buf, int64_t i) {
  return cfg_fraction_to_string(buf, i, 3);
}

static INLINE int
olsr_clock_fromIntervalString(uint64_t *result, const char *string) {
  int64_t t;
  int r;

  r = cfg_fraction_from_string(&t, string, 3);
  if (r == 0) {
    *result = t;
  }
  return r;
}

/**
 * Returns a timestamp s seconds in the future
 * @param s milliseconds until timestamp
 * @return absolute time when event will happen
 */
static INLINE uint64_t
olsr_clock_get_absolute(int64_t relative)
{
  return olsr_clock_getNow() + relative;
}

/**
 * Returns the number of milliseconds until the timestamp will happen
 * @param absolute timestamp
 * @return milliseconds until event will happen, negative if it already
 *   happened.
 */
static INLINE int64_t
olsr_clock_get_relative(uint64_t absolute)
{
  return (int64_t)absolute - (int64_t)olsr_clock_getNow();
}

/**
 * Checks if a timestamp has already happened
 * @param absolute timestamp
 * @return true if the event already happened, false otherwise
 */
static INLINE bool
olsr_clock_is_past(uint64_t absolute)
{
  return absolute < olsr_clock_getNow();
}

#endif

/*
 * Local Variables:
 * c-basic-offset: 2
 * indent-tabs-mode: nil
 * End:
 */

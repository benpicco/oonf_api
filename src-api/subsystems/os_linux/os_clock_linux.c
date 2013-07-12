
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

#include <time.h>

#include "common/common_types.h"
#include "core/oonf_subsystem.h"
#include "subsystems/os_clock.h"

/* prototypes */
static int _init(void);

/* type of clock source to be used */
#if defined(CLOCK_MONOTONIC_RAW) || defined (CLOCK_MONOTONIC)
static int _clock_source = 0;
#endif

/* subsystem definition */
struct oonf_subsystem oonf_os_clock_subsystem = {
  .name = "os_clock",
  .init = _init,
  .no_logging = true,
};

/**
 * Initialize os-specific subsystem
 * @return always return 0
 */
static int
_init(void) {
  struct timespec ts;

#ifdef CLOCK_MONOTONIC_RAW
  if (clock_gettime(CLOCK_MONOTONIC_RAW, &ts) == 0) {
    _clock_source = CLOCK_MONOTONIC_RAW;
  }
#endif
#ifdef CLOCK_MONOTONIC
  if (_clock_source == 0 && clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    _clock_source = CLOCK_MONOTONIC;
  }
#endif
  return 0;
}

/**
 * Reads the current time as a monotonic timestamp
 * @param t64 pointer to timestamp
 * @return 0 if valid timestamp was read, negative otherwise
 */
int
os_clock_gettime64(uint64_t *t64) {
  static time_t offset = 0, last_sec = 0;
  struct timeval tv;
  int error;

#if defined(CLOCK_MONOTONIC_RAW) || defined (CLOCK_MONOTONIC)
  if (_clock_source) {
    struct timespec ts;

    if ((error = clock_gettime(_clock_source, &ts)) != 0) {
      return error;
    }

    *t64 = 1000ull * ts.tv_sec + ts.tv_nsec / 1000000;
    return 0;
  }
#endif
  if ((error = gettimeofday(&tv, NULL)) != 0) {
    return error;
  }

  tv.tv_sec += offset;
  if (last_sec == 0) {
    last_sec = tv.tv_sec;
  }
  if (tv.tv_sec < last_sec || tv.tv_sec > last_sec + 60) {
    offset += last_sec - tv.tv_sec;
    tv.tv_sec = last_sec;
  }
  last_sec = tv.tv_sec;

  *t64 = 1000ull * tv.tv_sec + tv.tv_usec/ 1000;
  return 0;
}

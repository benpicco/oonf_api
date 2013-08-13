
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

#include <sys/times.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>

#include "core/oonf_libdata.h"
#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "core/os_core.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

/* subsystem definition */
struct oonf_subsystem oonf_os_core_subsystem = {
  .name = "os_core",
  .init = _init,
  .cleanup = _cleanup,
  .no_logging = true,
};

/**
 * Initialize core system
 * @return always returns 0
 */
static int
_init(void) {
  /* seed random number generator */
  srandom(times(NULL));

  /* open logfile */
  openlog(oonf_log_get_appdata()->app_name, LOG_PID | LOG_ODELAY, LOG_DAEMON);
  setlogmask(LOG_UPTO(LOG_DEBUG));

  return 0;
}

/**
 * Cleanup syslog system
 */
static void
_cleanup(void) {
  closelog();
}

/**
 * Print a line to the syslog
 * @param severity severity of entry
 * @param msg line to print
 */
void
os_core_syslog(enum oonf_log_severity severity, const char *msg) {
  int log_sev;

  switch (severity) {
    case LOG_SEVERITY_DEBUG:
      log_sev = LOG_DEBUG;
      break;
    case LOG_SEVERITY_INFO:
      log_sev = LOG_NOTICE;
      break;
    default:
    case LOG_SEVERITY_WARN:
      log_sev = LOG_WARNING;
      break;
  }

  syslog(log_sev, "%s", msg);
}

/**
 * Create a lock file of a certain name
 * @param path name of lockfile including path
 * @return 0 if the lock was created sucessfully, false otherwise
 */
int
os_core_create_lockfile(const char *path) {
  struct flock lck;
  int lock_fd;

  /* create file for lock */
  lock_fd = open(path, O_WRONLY | O_CREAT, S_IRWXU);
  if (lock_fd == -1) {
    return -1;
  }

  /* create exclusive lock for the whole file */
  lck.l_type = F_WRLCK;
  lck.l_whence = SEEK_SET;
  lck.l_start = 0;
  lck.l_len = 0;
  lck.l_pid = 0;

  if (fcntl(lock_fd, F_SETLK, &lck) == -1) {
    close(lock_fd);
    return -1;
  }

  /* lock will be released when process ends */
  return 0;
}

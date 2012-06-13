
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

#include <syslog.h>

#include "builddata/data.h"
#include "core/olsr_logging.h"
#include "core/os_system.h"
#include "core/olsr_subsystem.h"

#if OS_SYSTEM_LOG == OS_GENERIC
/* we only need this subsystem state here in the generic case */
OLSR_SUBSYSTEM_STATE(_os_log_state);
#endif

#if OS_SYSTEM_INIT == OS_GENERIC
/**
 * Initialize os-specific subsystem
 * @return -1 if an error happened, 0 otherwise
 */
int
os_system_init(void) {
  return 0;
}

/**
 * Cleanup os-specific subsystem
 */
void os_system_cleanup(void) {
}
#endif

#if OS_SYSTEM_INIT_IF == OS_GENERIC
/**
 * Initialize interface for mesh usage
 * @param interf pointer to interface object
 * @return -1 if an error happened, 0 otherwise
 */
int
os_routing_init_mesh_if(
    struct olsr_interface *interf __attribute__((unused))) {
  return 0;
}

/**
 * Cleanup interface after mesh usage
 * @param interf pointer to interface object
 */
void
os_routing_cleanup_mesh_if(
    struct olsr_interface *interf __attribute__((unused))) {
}
#endif

#if OS_SYSTEM_SET_IFSTATE == OS_GENERIC
/**
 * Set interface up or down
 * @param dev pointer to name of interface
 * @param up true if interface should be up, false if down
 * @return -1 if an error happened, 0 otherwise
 */
int
os_system_set_interface_state(
    const char *dev __attribute__((unused)),
    bool up __attribute__((unused))) {
  return -1;
}
#endif

#if OS_SYSTEM_LOG == OS_GENERIC
/**
 * Initialize syslog system
 */
void
os_system_openlog(void) {
  if (olsr_subsystem_init(&_os_log_state)) {
    return;
  }

  openlog(olsr_log_get_builddata()->app_name, LOG_PID | LOG_ODELAY, LOG_DAEMON);
  setlogmask(LOG_UPTO(LOG_DEBUG));

  return;
}

/**
 * Cleanup syslog system
 */
void
os_system_closelog(void) {
  if (olsr_subsystem_cleanup(&_os_log_state)) {
    return;
  }

  closelog();
}

/**
 * Print a line to the syslog
 * @param severity severity of entry
 * @param msg line to print
 */
void
os_system_log(enum log_severity severity, const char *msg) {
  int log_sev;

  switch (severity) {
    case LOG_SEVERITY_DEBUG:
      log_sev = LOG_DEBUG;
      break;
    case LOG_SEVERITY_INFO:
      log_sev = LOG_DEBUG;
      break;
    default:
    case LOG_SEVERITY_WARN:
      log_sev = LOG_WARNING;
      break;
  }

  syslog(log_sev, "%s", msg);
}
#endif

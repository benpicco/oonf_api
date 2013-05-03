
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
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "common/autobuf.h"
#include "common/list.h"
#include "common/string.h"
#include "core/olsr_libdata.h"
#include "core/olsr_logging.h"
#include "core/os_system.h"
#include "core/os_syslog.h"

#define FOR_ALL_LOGHANDLERS(handler, iterator) list_for_each_element_safe(&_handler_list, handler, node, iterator)

uint8_t log_global_mask[LOG_MAXIMUM_SOURCES];

static struct list_entity _handler_list;
static struct autobuf _logbuffer;
static const struct olsr_appdata *_appdata;
static const struct olsr_libdata *_libdata;
static uint8_t _default_mask;
static size_t _max_sourcetext_len, _max_severitytext_len, _source_count;

/* names for buildin logging targets */
const char *LOG_SOURCE_NAMES[LOG_MAXIMUM_SOURCES] = {
  [LOG_ALL]           = "all",
  [LOG_LOGGING]       = "logging",
  [LOG_CONFIG]        = "config",
  [LOG_MAIN]          = "main",
  [LOG_SOCKET]        = "socket",
  [LOG_TIMER]         = "timer",
  [LOG_CLASS]         = "class",
  [LOG_SOCKET_STREAM] = "socket-stream",
  [LOG_SOCKET_PACKET] = "socket-packet",
  [LOG_INTERFACE]     = "interface",
  [LOG_OS_NET]        = "os-net",
  [LOG_OS_SYSTEM]     = "os-system",
  [LOG_OS_ROUTING]    = "os-routing",
  [LOG_PLUGINLOADER]  = "plugin-loader",
  [LOG_TELNET]        = "telnet",
  [LOG_PLUGINS]       = "plugins",
  [LOG_HTTP]          = "http",
};

const char *LOG_SEVERITY_NAMES[LOG_SEVERITY_MAX+1] = {
  [LOG_SEVERITY_DEBUG] = "DEBUG",
  [LOG_SEVERITY_INFO]  = "INFO",
  [LOG_SEVERITY_WARN]  = "WARN",
};

/**
 * Initialize logging system
 * @param data builddata defined by application
 * @param def_severity default severity level
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_log_init(const struct olsr_appdata *data, enum log_severity def_severity)
{
  enum log_severity sev;
  enum log_source src;
  size_t len;

  _appdata = data;
  _libdata = olsr_libdata_get();
  _source_count = LOG_CORESOURCE_COUNT;

  list_init_head(&_handler_list);

  if (abuf_init(&_logbuffer)) {
    fputs("Not enough memory for logging buffer\n", stderr);
    return -1;
  }

  /* initialize maximum severity length */
  _max_severitytext_len = 0;
  OLSR_FOR_ALL_LOGSEVERITIES(sev) {
    len = strlen(LOG_SEVERITY_NAMES[sev]);
    if (len > _max_severitytext_len) {
      _max_severitytext_len = len;
    }
  }

  /* initialize maximum source length */
  _max_sourcetext_len = 0;
  for (src = 0; src < LOG_CORESOURCE_COUNT; src++) {
    len = strlen(LOG_SOURCE_NAMES[src]);
    if (len > _max_sourcetext_len) {
      _max_sourcetext_len = len;
    }
  }

  /* set default mask */
  _default_mask = 0;
  OLSR_FOR_ALL_LOGSEVERITIES(sev) {
    if (sev >= def_severity) {
      _default_mask |= sev;
    }
  }

  /* clear global mask */
  memset(&log_global_mask, _default_mask, sizeof(log_global_mask));

  return 0;
}

/**
 * Cleanup all resources allocated by logging system
 */
void
olsr_log_cleanup(void)
{
  struct log_handler_entry *h, *iterator;
  enum log_source src;

  /* remove all handlers */
  FOR_ALL_LOGHANDLERS(h, iterator) {
    olsr_log_removehandler(h);
  }

  for (src = LOG_CORESOURCE_COUNT; src < LOG_MAXIMUM_SOURCES; src++) {
    free ((void *)LOG_SOURCE_NAMES[src]);
    LOG_SOURCE_NAMES[src] = NULL;
  }
  abuf_free(&_logbuffer);
}

/**
 * Registers a custom logevent handler. Handler and bitmask_ptr have to
 * be initialized.
 * @param h pointer to log event handler struct
 * @return -1 if an out of memory error happened, 0 otherwise
 */
void
olsr_log_addhandler(struct log_handler_entry *h)
{
  list_add_tail(&_handler_list, &h->node);
  olsr_log_updatemask();
}

/**
 * Unregister a logevent handler
 * @param h pointer to handler entry
 */
void
olsr_log_removehandler(struct log_handler_entry *h)
{
  list_remove(&h->node);
  olsr_log_updatemask();
}

/**
 * register a new logging source in the logger
 * @param name pointer to the name of the logging source
 * @return index of the new logging source, LOG_MAIN if out of memory
 */
int
olsr_log_register_source(const char *name) {
  size_t i, len;

  /* maybe the source is already there ? */
  for (i=0; i<_source_count; i++) {
    if (strcmp(name, LOG_SOURCE_NAMES[i]) == 0) {
      return i;
    }
  }

  if (i == LOG_MAXIMUM_SOURCES) {
    OLSR_WARN(LOG_LOGGING, "Maximum number of logging sources reached,"
        " cannot allocate %s", name);
    return LOG_MAIN;
  }

  if ((LOG_SOURCE_NAMES[i] = strdup(name)) == NULL) {
    OLSR_WARN(LOG_LOGGING, "Not enough memory for duplicating source name %s", name);
    return LOG_MAIN;
  }

  _source_count++;
  len = strlen(name);
  if (len > _max_sourcetext_len) {
    _max_sourcetext_len = len;
  }
  return i;
}

/**
 * @return maximum text length of a log severity string
 */
size_t
olsr_log_get_max_severitytextlen(void) {
  return _max_severitytext_len;
}

/**
 * @return maximum text length of a log source string
 */
size_t
olsr_log_get_max_sourcetextlen(void) {
  return _max_sourcetext_len;
}

/**
 * @return current number of logging sources
 */
size_t
olsr_log_get_sourcecount(void) {
  return _source_count;
}

/**
 * @return pointer to application data
 */
const struct olsr_appdata *
olsr_log_get_appdata(void) {
  return _appdata;
}

/**
 * @return pointer to library data
 */
const struct olsr_libdata *
olsr_log_get_libdata(void) {
  return _libdata;
}

/**
 * Print version string
 * @param abuf target output buffer
 */
void
olsr_log_printversion(struct autobuf *abuf) {
  abuf_appendf(abuf," %s version %s (%s)\n"
            " Built on %s\n"
            " Application commit: %s\n",
            _appdata->app_name,
            _appdata->app_version,
            _appdata->builddate, _appdata->buildsystem,
            _appdata->git_commit);
  if (_appdata->git_change[0]) {
    abuf_appendf(abuf, "      %s\n", _appdata->git_change);
  }
  abuf_appendf(abuf, " Library commit: %s\n", _libdata->git_commit);
  if (_libdata->git_change[0]) {
    abuf_appendf(abuf, "      %s\n", _libdata->git_change);
  }
  abuf_puts(abuf, _appdata->versionstring_trailer);
}

/**
 * Recalculate the combination of the olsr_cnf log event mask and all (if any)
 * custom masks of logfile handlers. Must be called every times a event mask
 * changes without a logevent handler being added or removed.
 */
void
olsr_log_updatemask(void)
{
  enum log_source src;
  struct log_handler_entry *h, *iterator;
  uint8_t mask;

  /* first reset global mask */
  olsr_log_mask_clear(log_global_mask);

  FOR_ALL_LOGHANDLERS(h, iterator) {
    for (src = 0; src < LOG_MAXIMUM_SOURCES; src++) {
      /* copy user defined mask */
      mask = h->user_bitmask[src];

      /* apply 'all' source mask */
      mask |= h->user_bitmask[LOG_ALL];

      /* propagate severities from lower to higher level */
      mask |= mask << 1;
      mask |= mask << 2;

      /*
       * we don't need the third shift because we have
       * 4 or less severity level
       */
#if 0
      mask |= mask << 4;
#endif

      /* write calculated mask into internal buffer */
      h->_processed_bitmask[src] = mask;

      /* apply calculated mask to the global one */
      log_global_mask[src] |= mask;
    }
  }
}

/**
 * @return pointer to string containing the current walltime
 */
const char *
olsr_log_get_walltime(void) {
  static char buf[sizeof("00:00:00.000")];
  struct timeval now;
  struct tm *tm;

  if (os_system_gettimeofday(&now)) {
    return NULL;
  }

  tm = localtime(&now.tv_sec);
  if (tm == NULL) {
    return NULL;
  }
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03ld",
      tm->tm_hour, tm->tm_min, tm->tm_sec, now.tv_usec / 1000);

  return buf;
}

/**
 * This function should not be called directly, use the macros OLSR_{DEBUG,INFO,WARN} !
 *
 * Generates a logfile entry and calls all log handler to store/output it.
 *
 * @param severity severity of the log event (LOG_SEVERITY_DEBUG to LOG_SEVERITY_WARN)
 * @param source source of the log event (LOG_LOGGING, ... )
 * @param no_header true if time header should not be created
 * @param file filename where the logging macro have been called
 * @param line line number where the logging macro have been called
 * @param format printf format string for log output plus a variable number of arguments
 */
void
olsr_log(enum log_severity severity, enum log_source source, bool no_header,
    const char *file, int line, const char *format, ...)
{
  struct log_handler_entry *h, *iterator;
  struct log_parameters param;
  va_list ap;
  int p1 = 0, p2 = 0, p3 = 0;

  va_start(ap, format);

  /* generate log string */
  abuf_clear(&_logbuffer);
  if (!no_header) {
    p1 = abuf_appendf(&_logbuffer, "%s ", olsr_log_get_walltime());
    p2 = abuf_appendf(&_logbuffer, "%s(%s) %s %d: ",
        LOG_SEVERITY_NAMES[severity], LOG_SOURCE_NAMES[source], file, line);
  }
  p3 = abuf_vappendf(&_logbuffer, format, ap);

  /* remove \n at the end of the line if necessary */
  if (abuf_getptr(&_logbuffer)[p1 + p2 + p3 - 1] == '\n') {
    abuf_getptr(&_logbuffer)[p1 + p2 + p3 - 1] = 0;
  }

  param.severity = severity;
  param.source = source;
  param.no_header = no_header;
  param.file = file;
  param.line = line;
  param.buffer = abuf_getptr(&_logbuffer);
  param.timeLength = p1;
  param.prefixLength = p2;

  /* use stderr logger if nothing has been configured */
  if (list_is_empty(&_handler_list)) {
    olsr_log_stderr(NULL, &param);
    return;
  }

  /* call all log handlers */
  FOR_ALL_LOGHANDLERS(h, iterator) {
    if (olsr_log_mask_test(h->_processed_bitmask, source, severity)) {
      h->handler(h, &param);
    }
  }
  va_end(ap);
}

/**
 * Logger for stderr output
 * @param entry logging handler, might be NULL because this is the
 *   default logger
 * @param param logging parameter set
 */
void
olsr_log_stderr(struct log_handler_entry *entry __attribute__ ((unused)),
    struct log_parameters *param)
{
  fputs(param->buffer, stderr);
  fputc('\n', stderr);
}

/**
 * Logger for file output
 * @param entry logging handler
 * @param param logging parameter set
 */
void
olsr_log_file(struct log_handler_entry *entry,
    struct log_parameters *param)
{
  FILE *f;

  f = entry->custom;
  fputs(param->buffer, f);
  fputc('\n', f);
  fflush(f);
}

/**
 * Logger for syslog output
 * @param entry logging handler, might be NULL
 * @param param logging parameter set
 */
void
olsr_log_syslog(struct log_handler_entry *entry __attribute__ ((unused)),
    struct log_parameters *param)
{
  os_syslog_log(param->severity, param->buffer + param->timeLength);
}

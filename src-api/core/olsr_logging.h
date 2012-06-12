
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

#ifndef OLSR_LOGGING_H_
#define OLSR_LOGGING_H_

struct log_handler_entry;

#include "string.h"

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/list.h"
#include "olsr_libdata.h"

/**
 * defines the severity of a logging event
 */
enum log_severity {
  LOG_SEVERITY_MIN   = 1<<0,
  LOG_SEVERITY_DEBUG = 1<<0,
  LOG_SEVERITY_INFO  = 1<<1,
  LOG_SEVERITY_WARN  = 1<<2,
  LOG_SEVERITY_MAX   = 1<<2,
};

/* Defines the builtin sources of a logging event. */
enum log_source {
  LOG_ALL,
  LOG_LOGGING,
  LOG_CONFIG,
  LOG_MAIN,
  LOG_SOCKET,
  LOG_TIMER,
  LOG_MEMCOOKIE,
  LOG_SOCKET_STREAM,
  LOG_SOCKET_PACKET,
  LOG_INTERFACE,
  LOG_OS_NET,
  LOG_OS_SYSTEM,
  LOG_OS_ROUTING,
  LOG_PLUGINLOADER,
  LOG_TELNET,
  LOG_PLUGINS,
  LOG_HTTP,

  /* this one must be the last ones of the normal enums ! */
  LOG_CORESOURCE_COUNT,

  /* maximum number of logging sources supported by API */
  LOG_MAXIMUM_SOURCES = 64,
};

struct log_parameters {
  enum log_severity severity;
  enum log_source source;
  bool no_header;
  const char *file;
  int line;
  char *buffer;
  int timeLength;
  int prefixLength;
};

struct olsr_appdata {
  const char *app_name;
  const char *app_version;
  const char *versionstring_trailer;
  const char *help_prefix;
  const char *help_suffix;

  const char *default_config;

  const char *git_commit;
  const char *git_change;

  const char *builddate;
  const char *buildsystem;

  const char *sharedlibrary_prefix;
  const char *sharedlibrary_postfix;
};

/**
 * these macros should be used to generate OLSR logging output
 * the OLSR_severity_NH() variants don't print the timestamp/file/line header,
 * which can be used for multiline output.
 *
 * OLSR_DEBUG should be used for all output that is only usefull for debugging a specific
 * part of the code. This could be information about the internal progress of a function,
 * state of variables, ...
 *
 * OLSR_INFO should be used for all output that does not inform the user about a
 * problem/error in OLSR. Examples would be "SPF run triggered" or "Hello package received
 * from XXX.XXX.XXX.XXX".
 *
 * OLSR_WARN should be used for all error messages.
 */

#define _OLSR_LOG(severity, source, no_header, format, args...) do { if (olsr_log_mask_test(log_global_mask, source, severity)) olsr_log(severity, source, no_header, __FILE__, __LINE__, format, ##args); } while(0)

#if MAX_LOGGING_LEVEL < 1
#define OLSR_DEBUG(source, format, args...) do { } while(0)
#define OLSR_DEBUG_NH(source, format, args...) do { } while(0)
#else
#define OLSR_DEBUG(source, format, args...) _OLSR_LOG(LOG_SEVERITY_DEBUG, source, false, format, ##args)
#define OLSR_DEBUG_NH(source, format, args...) _OLSR_LOG(LOG_SEVERITY_DEBUG, source, true, format, ##args)
#endif

#if MAX_LOGGING_LEVEL < 2
#define OLSR_INFO(source, format, args...) do { } while(0)
#define OLSR_INFO_NH(source, format, args...) do { } while(0)
#else
#define OLSR_INFO(source, format, args...) _OLSR_LOG(LOG_SEVERITY_INFO, source, false, format, ##args)
#define OLSR_INFO_NH(source, format, args...) _OLSR_LOG(LOG_SEVERITY_INFO, source, true, format, ##args)
#endif

#if MAX_LOGGING_LEVEL < 3
#define OLSR_WARN(source, format, args...) do { } while(0)
#define OLSR_WARN_NH(source, format, args...) do { } while(0)
#else
#define OLSR_WARN(source, format, args...) _OLSR_LOG(LOG_SEVERITY_WARN, source, false, format, ##args)
#define OLSR_WARN_NH(source, format, args...) _OLSR_LOG(LOG_SEVERITY_WARN, source, true, format, ##args)
#endif

typedef void log_handler_cb(struct log_handler_entry *, struct log_parameters *);

struct log_handler_entry {
  struct list_entity node;
  log_handler_cb *handler;

  /* user_bitmask */
  uint8_t user_bitmask[LOG_MAXIMUM_SOURCES];

  /* internal copy of user_bitmask */
  uint8_t _processed_bitmask[LOG_MAXIMUM_SOURCES];

  /* custom data for user */
  void *custom;
};

#define OLSR_FOR_ALL_LOGSEVERITIES(sev) for (sev = LOG_SEVERITY_MIN; sev <= LOG_SEVERITY_MAX; sev <<= 1)

EXPORT extern uint8_t log_global_mask[LOG_MAXIMUM_SOURCES];
EXPORT extern const char *LOG_SOURCE_NAMES[LOG_MAXIMUM_SOURCES];
EXPORT extern const char *LOG_SEVERITY_NAMES[LOG_SEVERITY_MAX+1];

EXPORT int olsr_log_init(const struct olsr_appdata *, enum log_severity)
  __attribute__((warn_unused_result));
EXPORT void olsr_log_cleanup(void);

EXPORT size_t olsr_log_get_max_severitytextlen(void);
EXPORT size_t olsr_log_get_max_sourcetextlen(void);
EXPORT size_t olsr_log_get_sourcecount(void);

EXPORT void olsr_log_addhandler(struct log_handler_entry *);
EXPORT void olsr_log_removehandler(struct log_handler_entry *);
EXPORT int olsr_log_register_source(const char *name);
EXPORT void olsr_log_unregister_source(int index);

EXPORT void olsr_log_updatemask(void);

EXPORT const struct olsr_appdata *olsr_log_get_appdata(void);
EXPORT const struct olsr_libdata *olsr_log_get_libdata(void);
EXPORT void olsr_log_printversion(struct autobuf *abuf);

EXPORT const char *olsr_log_get_walltime(void);

EXPORT void olsr_log(enum log_severity, enum log_source, bool, const char *, int, const char *, ...)
  __attribute__ ((format(printf, 6, 7)));

EXPORT void olsr_log_stderr(struct log_handler_entry *,
    struct log_parameters *);
EXPORT void olsr_log_syslog(struct log_handler_entry *,
    struct log_parameters *);
EXPORT void olsr_log_file(struct log_handler_entry *,
    struct log_parameters *);

/**
 * Clear a logging mask
 * @param mask pointer to logging mask
 */
static INLINE void
olsr_log_mask_clear(uint8_t *mask) {
  memset(mask, 0, LOG_MAXIMUM_SOURCES);
}

/**
 * Copy a logging mask
 * @param dst pointer to target logging mask
 * @param src pointer to source logging mask
 */
static INLINE void
olsr_log_mask_copy(uint8_t *dst, uint8_t *src) {
  memcpy(dst, src, LOG_MAXIMUM_SOURCES);
}

/**
 * Set a field in a logging mask
 * @param mask pointer to logging mask
 * @param src logging source
 * @param sev logging severity
 */
static INLINE void
olsr_log_mask_set(uint8_t *mask, enum log_source src, enum log_severity sev) {
  mask[src] |= sev;
}

/**
 * Reset a field in a logging mask
 * @param mask pointer to logging mask
 * @param src logging source
 * @param sev logging severity
 */
static INLINE void
olsr_log_mask_reset(uint8_t *mask, enum log_source src, enum log_severity sev) {
  mask[src] &= ~sev;
}

/**
 * Test a field in a logging mask
 * @param mask pointer to logging mask
 * @param src logging source
 * @param sev logging severity
 * @return true if the field was set, false otherwise
 */
static INLINE bool
olsr_log_mask_test(uint8_t *mask, enum log_source src, enum log_severity sev) {
  return (mask[src] & sev) != 0;
}

#endif /* OLSR_LOGGING_H_ */

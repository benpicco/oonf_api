
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

#ifndef OONF_LOGGING_H_
#define OONF_LOGGING_H_

struct log_handler_entry;

#include "string.h"

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/list.h"
#include "core/oonf_libdata.h"

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
  /* all logging sources */
  LOG_ALL,

  /* the 'default' logging source */
  LOG_MAIN,

  /* logging sources for the core */
  LOG_LOGGING,
  LOG_CONFIG,
  LOG_PLUGINS,

  /* logging sources for subsystems */
  LOG_CLASS,
  LOG_CLOCK,
  // LOG_DUPLICATE_SET,
  LOG_HTTP,
  LOG_INTERFACE,
  LOG_LAYER2,
  LOG_LINKCONFIG,
  LOG_PACKET,
  LOG_RFC5444,
  LOG_SOCKET,
  LOG_STREAM,
  LOG_TELNET,
  LOG_TIMER,

  LOG_OS_NET,
  LOG_OS_ROUTING,
  LOG_OS_SYSTEM,

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

struct oonf_appdata {
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

/*
 * macros to check which logging levels are active
 *
 * #if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_xxxx
 *   // variable only necessary for logging level xxx and finer
 *   struct netaddr_str buf;
 * #endif
 */

#define OONF_LOGGING_LEVEL_WARN  0
#define OONF_LOGGING_LEVEL_INFO  2
#define OONF_LOGGING_LEVEL_DEBUG 3

/**
 * these macros should be used to generate OONF logging output
 * the OONF_severity_NH() variants don't print the timestamp/file/line header,
 * which can be used for multiline output.
 *
 * OONF_DEBUG should be used for all output that is only usefull for debugging a specific
 * part of the code. This could be information about the internal progress of a function,
 * state of variables, ...
 *
 * OONF_INFO should be used for all output that does not inform the user about a
 * problem/error in OONF. Examples would be "SPF run triggered" or "Hello package received
 * from XXX.XXX.XXX.XXX".
 *
 * OONF_WARN should be used for all error messages.
 */

#define _OONF_LOG(severity, source, no_header, format, args...) do { if (oonf_log_mask_test(log_global_mask, source, severity)) oonf_log(severity, source, no_header, __FILE__, __LINE__, format, ##args); } while(0)

#if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_DEBUG
#define OONF_DEBUG(source, format, args...) _OONF_LOG(LOG_SEVERITY_DEBUG, source, false, format, ##args)
#define OONF_DEBUG_NH(source, format, args...) _OONF_LOG(LOG_SEVERITY_DEBUG, source, true, format, ##args)
#else
#define OONF_DEBUG(source, format, args...) do { } while(0)
#define OONF_DEBUG_NH(source, format, args...) do { } while(0)
#endif

#if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_INFO
#define OONF_INFO(source, format, args...) _OONF_LOG(LOG_SEVERITY_INFO, source, false, format, ##args)
#define OONF_INFO_NH(source, format, args...) _OONF_LOG(LOG_SEVERITY_INFO, source, true, format, ##args)
#else
#define OONF_INFO(source, format, args...) do { } while(0)
#define OONF_INFO_NH(source, format, args...) do { } while(0)
#endif

#define OONF_WARN(source, format, args...) _OONF_LOG(LOG_SEVERITY_WARN, source, false, format, ##args)
#define OONF_WARN_NH(source, format, args...) _OONF_LOG(LOG_SEVERITY_WARN, source, true, format, ##args)

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

#define OONF_FOR_ALL_LOGSEVERITIES(sev) for (sev = LOG_SEVERITY_MIN; sev <= LOG_SEVERITY_MAX; sev <<= 1)

EXPORT extern uint8_t log_global_mask[LOG_MAXIMUM_SOURCES];
EXPORT extern const char *LOG_SOURCE_NAMES[LOG_MAXIMUM_SOURCES];
EXPORT extern const char *LOG_SEVERITY_NAMES[LOG_SEVERITY_MAX+1];

EXPORT int oonf_log_init(const struct oonf_appdata *, enum log_severity)
  __attribute__((warn_unused_result));
EXPORT void oonf_log_cleanup(void);

EXPORT size_t oonf_log_get_max_severitytextlen(void);
EXPORT size_t oonf_log_get_max_sourcetextlen(void);
EXPORT size_t oonf_log_get_sourcecount(void);

EXPORT void oonf_log_addhandler(struct log_handler_entry *);
EXPORT void oonf_log_removehandler(struct log_handler_entry *);
EXPORT int oonf_log_register_source(const char *name);

EXPORT void oonf_log_updatemask(void);

EXPORT const struct oonf_appdata *oonf_log_get_appdata(void);
EXPORT const struct oonf_libdata *oonf_log_get_libdata(void);
EXPORT void oonf_log_printversion(struct autobuf *abuf);

EXPORT const char *oonf_log_get_walltime(void);

EXPORT void oonf_log(enum log_severity, enum log_source, bool, const char *, int, const char *, ...)
  __attribute__ ((format(printf, 6, 7)));

EXPORT void oonf_log_stderr(struct log_handler_entry *,
    struct log_parameters *);
EXPORT void oonf_log_syslog(struct log_handler_entry *,
    struct log_parameters *);
EXPORT void oonf_log_file(struct log_handler_entry *,
    struct log_parameters *);

/**
 * Clear a logging mask
 * @param mask pointer to logging mask
 */
static INLINE void
oonf_log_mask_clear(uint8_t *mask) {
  memset(mask, 0, LOG_MAXIMUM_SOURCES);
}

/**
 * Copy a logging mask
 * @param dst pointer to target logging mask
 * @param src pointer to source logging mask
 */
static INLINE void
oonf_log_mask_copy(uint8_t *dst, uint8_t *src) {
  memcpy(dst, src, LOG_MAXIMUM_SOURCES);
}

/**
 * Set a field in a logging mask
 * @param mask pointer to logging mask
 * @param src logging source
 * @param sev logging severity
 */
static INLINE void
oonf_log_mask_set(uint8_t *mask, enum log_source src, enum log_severity sev) {
  mask[src] |= sev;
}

/**
 * Reset a field in a logging mask
 * @param mask pointer to logging mask
 * @param src logging source
 * @param sev logging severity
 */
static INLINE void
oonf_log_mask_reset(uint8_t *mask, enum log_source src, enum log_severity sev) {
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
oonf_log_mask_test(uint8_t *mask, enum log_source src, enum log_severity sev) {
  return (mask[src] & sev) != 0;
}

#endif /* OONF_LOGGING_H_ */

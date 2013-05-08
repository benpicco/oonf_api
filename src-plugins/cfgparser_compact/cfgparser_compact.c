
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

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/string.h"
#include "config/cfg_db.h"
#include "config/cfg_parser.h"
#include "config/cfg.h"
#include "core/oonf_subsystem.h"
#include "core/oonf_plugins.h"
#include "core/oonf_cfg.h"

#include "cfgparser_compact/cfgparser_compact.h"

static void _early_cfg_init(void);
static void _cleanup(void);

static struct cfg_db *_cb_compact_parse(
    char *src, size_t len, struct autobuf *log);
static int _cb_compact_serialize(
    struct autobuf *dst, struct cfg_db *src, struct autobuf *log);
static int _parse_line(struct cfg_db *db, char *line,
    char *section, size_t section_size,
    char *name, size_t name_size,
    struct autobuf *log);

struct oonf_subsystem oonf_compact_parser_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONFD compact configuration format plugin",
  .author = "Henning Rogge",

  .early_cfg_init = _early_cfg_init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(oonf_compact_parser_subsystem);

struct cfg_parser cfg_parser_compact = {
  .name = "compact",
  .parse = _cb_compact_parse,
  .serialize = _cb_compact_serialize,
  .def = true
};

/**
 * Callback to hook plugin into configuration system.
 */
static void
_early_cfg_init(void)
{
  cfg_parser_add(oonf_cfg_get_instance(), &cfg_parser_compact);
}

/**
 * Destructor of plugin
 */
static void
_cleanup(void)
{
  cfg_parser_remove(oonf_cfg_get_instance(), &cfg_parser_compact);
}


/*
 * Defintion of the 'compact' configuration parser.
 *
 * This parser/serializer use a line oriented text format.
 * All lines beginning with '#' will be ignored as comments.
 * All leading and trailing whitespaces will be ignored.
 *
 * Sections are defined as '[<section-type>]'.
 * Named sections are defined as '[<section-type>=<section-name>]'.
 * Entrys are defined as 'key value'.
 *
 * No entry must be put before the first section.
 */

/**
 * Parse a buffer into a configuration database
 * @param src pointer to text buffer
 * @param len length of buffer
 * @param log autobuffer for logging output
 * @return pointer to configuration database, NULL if an error happened
 */
static struct cfg_db *
_cb_compact_parse(char *src, size_t len, struct autobuf *log) {
  char section[128];
  char name[128];
  struct cfg_db *db;
  char *eol, *line;

  db = cfg_db_add();
  if (!db) {
    return NULL;
  }

  memset(section, 0, sizeof(section));
  memset(name, 0, sizeof(name));

  line = src;
  while (line < src + len) {
    /* find end of line */
    eol = line;
    while (*eol != 0 && *eol != '\n') {
      eol++;
    }

    /* termiate line with zero byte */
    *eol = 0;
    if (eol > line && eol[-1] == '\r') {
      /* handle \r\n line ending */
      eol[-1] = 0;
    }

    if (_parse_line(db, line, section, sizeof(section),
        name, sizeof(name), log)) {
      cfg_db_remove(db);
      return NULL;
    }

    line = eol+1;
  }

  return db;
}

/**
 * Serialize a configuration database into a buffer
 * @param dst target buffer
 * @param src source configuration database
 * @param log autbuffer for logging
 * @return 0 if database was serialized, -1 otherwise
 */
static int
_cb_compact_serialize(struct autobuf *dst, struct cfg_db *src,
    struct autobuf *log __attribute__ ((unused))) {
  struct cfg_section_type *section, *s_it;
  struct cfg_named_section *name, *n_it;
  struct cfg_entry *entry, *e_it;
  char *ptr;

  CFG_FOR_ALL_SECTION_TYPES(src, section, s_it) {
    CFG_FOR_ALL_SECTION_NAMES(section, name, n_it) {
      if (cfg_db_is_named_section(name)) {
        abuf_appendf(dst, "[%s=%s]\n", section->type, name->name);
      }
      else {
        abuf_appendf(dst, "[%s]\n", section->type);
      }

      CFG_FOR_ALL_ENTRIES(name, entry, e_it) {
        FOR_ALL_STRINGS(&entry->val, ptr) {
          abuf_appendf(dst, "\t%s %s\n", entry->name, ptr);
        }
      }
    }
  }
  return 0;
}

/**
 * Parse a single line of the compact format
 * @param db pointer to configuration database
 * @param line pointer to line to be parsed (will be modified
 *   during parsing)
 * @param section pointer to array with current section type
 *   (might be modified during parsing)
 * @param section_size number of bytes for section type
 * @param name pointer to array with current section name
 *   (might be modified during parsing)
 * @param name_size number of bytes for section name
 * @param log autobuffer for logging output
 * @return 0 if line was parsed successfully, -1 otherwise
 */
static int
_parse_line(struct cfg_db *db, char *line,
    char *section, size_t section_size,
    char *name, size_t name_size,
    struct autobuf *log) {
  char *first, *ptr;
  bool dummy;

  /* trim leading and trailing whitespaces */
  first = str_trim(line);

  if (*first == 0 || *first == '#') {
    /* empty line or comment */
    return 0;
  }

  if (*first == '[') {
    first++;
    ptr = strchr(first, ']');
    if (ptr == NULL) {
      cfg_append_printable_line(log,
          "Section syntax error in line: '%s'", line);
      return -1;
    }
    *ptr = 0;

    ptr = strchr(first, '=');
    if (ptr) {
      /* trim section name */
      *ptr++ = 0;
      ptr = str_trim(ptr);
    }

    /* trim section name */
    first = str_trim(first);
    if (*first == 0) {
      cfg_append_printable_line(log,
          "Section syntax error, no section type found");
      return -1;
    }

    /* copy section type */
    strscpy(section, first, section_size);

    /* copy section name */
    if (ptr) {
      strscpy(name, ptr, name_size);
    }
    else {
      *name = 0;
    }

    /* add section to db */
    if (_cfg_db_add_section(db, section, *name ? name : NULL, &dummy) == NULL) {
      return -1;
    }
    return 0;
  }

  if (*section == 0) {
    cfg_append_printable_line(log,
        "Entry before first section is not allowed in this format");
    return -1;
  }

  ptr = first;

  /* look for separator */
  while (!isspace(*ptr)) {
    ptr++;
  }

  *ptr++ = 0;

  /* trim second token */
  ptr = str_trim(ptr);

  if (*ptr == 0) {
    cfg_append_printable_line(log,
        "No second token found in line '%s'",  line);
    return -1;
  }

  /* found two tokens */
  if (cfg_db_add_entry(db, section, *name ? name : NULL, first, ptr)) {
    return -1;
  }
  return 0;
}

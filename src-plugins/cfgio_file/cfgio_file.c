
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

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "common/autobuf.h"
#include "config/cfg_io.h"
#include "config/cfg_parser.h"
#include "config/cfg.h"
#include "core/olsr_plugins.h"

#include "core/olsr_cfg.h"

#include "cfgio_file/cfgio_file.h"

static void _early_cfg_init(void);
static void _cleanup(void);

static struct cfg_db *_cb_file_load(struct cfg_instance *instance,
    const char *param, const char *parser, struct autobuf *log);
static int _cb_file_save(struct cfg_instance *instance,
    const char *param, const char *parser, struct cfg_db *src, struct autobuf *log);

struct oonf_subsystem oonf_io_file_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OLSRD file io handler for configuration system",
  .author = "Henning Rogge",

  .cleanup = _cleanup,
  .early_cfg_init = _early_cfg_init,
};
DECLARE_OONF_PLUGIN(oonf_io_file_subsystem);

struct cfg_io cfg_io_file = {
  .name = "file",
  .load = _cb_file_load,
  .save = _cb_file_save,
  .def = true,
};

/**
 * Callback to hook plugin into configuration system.
 */
static void
_early_cfg_init(void)
{
  cfg_io_add(olsr_cfg_get_instance(), &cfg_io_file);
}

/**
 * Destructor of plugin
 */
static void
_cleanup(void)
{
  cfg_io_remove(olsr_cfg_get_instance(), &cfg_io_file);
}

/*
 * Definition of the file-io handler.
 *
 * This handler can read and write files and use a parser to
 * translate them into a configuration database (and the other way around)
 *
 * The parameter of this parser has to be a filename
 */

/**
 * Reads a file from a filesystem, parse it with the help of a
 * configuration parser and returns a configuration database.
 * @param parser parser name, NULL if autodetection should be used
 * @param param file to be read
 * @param log autobuffer for logging purpose
 * @return pointer to configuration database, NULL if an error happened
 */
static struct cfg_db *
_cb_file_load(struct cfg_instance *instance,
    const char *param, const char *parser, struct autobuf *log) {
  struct autobuf dst;
  struct cfg_db *db;
  char buffer[1024];
  int fd = 0;
  ssize_t bytes;

  fd = open(param, O_RDONLY, 0);
  if (fd == -1) {
    cfg_append_printable_line(log,
        "Cannot open file '%s' to read configuration: %s (%d)",
        param, strerror(errno), errno);
    return NULL;
  }

  bytes = 1;
  if (abuf_init(&dst)) {
    cfg_append_printable_line(log,
        "Out of memory error while allocating io buffer");
    close (fd);
    return NULL;
  }

  /* read file into binary buffer */
  while (bytes > 0) {
    bytes = read(fd, buffer, sizeof(buffer));
    if (bytes < 0 && errno != EINTR) {
      cfg_append_printable_line(log,
          "Error while reading file '%s': %s (%d)",
          param, strerror(errno), errno);
      close(fd);
      abuf_free(&dst);
      return NULL;
    }

    if (bytes > 0) {
      abuf_memcpy(&dst, buffer, (size_t)bytes);
    }
  }
  close(fd);

  if (parser == NULL) {
    /* lookup a fitting parser, we know the path as a hint */
    parser = cfg_parser_find(instance, &dst, param, NULL);
  }

  db = cfg_parser_parse_buffer(instance, parser, abuf_getptr(&dst), abuf_getlen(&dst), log);
  abuf_free(&dst);
  return db;
}

/**
 * Stores a configuration database into a file. It will use a
 * parser (the serialization part) to translate the database into
 * a storage format.
 * @param parser parser name, NULL if autodetection should be used
 * @param param pathname to write configuration file into
 * @param src_db source configuration database
 * @param log autobuffer for logging purpose
 * @return 0 if database was stored sucessfully, -1 otherwise
 */
static int
_cb_file_save(struct cfg_instance *instance,
    const char *param, const char *parser,
    struct cfg_db *src_db, struct autobuf *log) {
  int fd = 0;
  ssize_t bytes;
  size_t total;
  struct autobuf abuf;

  if (abuf_init(&abuf)) {
    cfg_append_printable_line(log,
        "Out of memory error while allocating io buffer");
    return -1;
  }

  if (parser == NULL) {
    parser = cfg_parser_find(instance, NULL, param, NULL);
  }
  if (cfg_parser_serialize_to_buffer(instance, parser, &abuf, src_db, log)) {
    abuf_free(&abuf);
    return -1;
  }

  fd = open(param, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR);
  if (fd == -1) {
    cfg_append_printable_line(log,
        "Cannot open file '%s' for writing configuration: %s (%d)",
        param, strerror(errno), errno);
    return -1;
  }

  total = 0;
  while (total < abuf_getlen(&abuf)) {
    bytes = write(fd, abuf_getptr(&abuf) + total, abuf_getlen(&abuf) - total);
    if (bytes <= 0 && errno != EINTR) {
      cfg_append_printable_line(log,
          "Error while writing to file '%s': %s (%d)",
          param, strerror(errno), errno);
      close(fd);
      return -1;
    }

    if (bytes > 0) {
      total += (size_t)bytes;
    }
  }
  close(fd);
  abuf_free(&abuf);

  return 0;
}

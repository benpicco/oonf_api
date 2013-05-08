
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

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "config/cfg_schema.h"
#include "core/oonf_logging.h"
#include "core/oonf_plugins.h"
#include "core/oonf_subsystem.h"
#include "core/oonf_cfg.h"
#include "subsystems/oonf_http.h"
#include "subsystems/oonf_telnet.h"

/* constants */
#define _CFG_SECTION "httptelnet"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static enum oonf_http_result _cb_generate_site(
    struct autobuf *out, struct oonf_http_session *);

static void _cb_config_changed(void);

/* html handler */
static struct oonf_http_handler _http_site_handler = {
  .content_handler = _cb_generate_site,
};

/* configuration */
static struct cfg_schema_entry _httptelnet_entries[] = {
  CFG_MAP_STRING(oonf_http_handler, site, "site", "/telnet", "Path for http2telnet bridge"),
  CFG_MAP_ACL(oonf_http_handler, acl, "acl", "default_accept", "acl for http2telnet bridge"),
  CFG_MAP_STRINGLIST(oonf_http_handler, auth, "auth", "", "TODO"),
};

static struct cfg_schema_section _httptelnet_section = {
  .type = _CFG_SECTION,
  .cb_delta_handler = _cb_config_changed,
  .entries = _httptelnet_entries,
  .entry_count = ARRAYSIZE(_httptelnet_entries),
};

/* plugin declaration */
struct oonf_subsystem oonf_httptelnet_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONFD http2telnet bridge plugin",
  .author = "Henning Rogge",

  .cfg_section = &_httptelnet_section,

  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(oonf_httptelnet_subsystem);

/**
 * Constructor of plugin
 * @return 0 if initialization was successful, -1 otherwise
 */
static int
_init(void) {
  netaddr_acl_add(&_http_site_handler.acl);
  strarray_init(&_http_site_handler.auth);

  return 0;
}

/**
 * Destructor of plugin
 */
static void
_cleanup(void) {
  strarray_free(&_http_site_handler.auth);
  netaddr_acl_remove(&_http_site_handler.acl);
  free((char *)_http_site_handler.site);
}

/**
 * Callback for generating a http site from the output of the
 * triggered telnet command
 * @param out pointer to output buffer
 * @param session pointer to http session
 * @return http result code
 */
static enum oonf_http_result
_cb_generate_site(struct autobuf *out, struct oonf_http_session *session) {
  const char *command, *param;

  command = oonf_http_lookup_param(session, "c");
  param = oonf_http_lookup_param(session, "p");

  if (command == NULL) {
    return HTTP_404_NOT_FOUND;
  }

  switch (oonf_telnet_execute(command, param, out, session->remote)) {
    case TELNET_RESULT_ACTIVE:
    case TELNET_RESULT_QUIT:
      session->content_type = HTTP_CONTENTTYPE_TEXT;
      return HTTP_200_OK;

    case _TELNET_RESULT_UNKNOWN_COMMAND:
      return HTTP_404_NOT_FOUND;

    default:
      return HTTP_400_BAD_REQ;
  }
}

/**
 * Update configuration of remotecontrol plugin
 */
static void
_cb_config_changed(void) {
  if (cfg_schema_tobin(&_http_site_handler, _httptelnet_section.post,
      _httptelnet_entries, ARRAYSIZE(_httptelnet_entries))) {
    OONF_WARN(LOG_CONFIG, "Could not convert httptelnet config to bin");
    return;
  }

  if (_httptelnet_section.pre) {
    oonf_http_remove(&_http_site_handler);
  }
  if (_httptelnet_section.post) {
    oonf_http_add(&_http_site_handler);
  }
}

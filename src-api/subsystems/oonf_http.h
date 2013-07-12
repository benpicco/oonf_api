
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

#ifndef OONF_HTTP_H_
#define OONF_HTTP_H_

#include "common/avl.h"
#include "common/common_types.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "common/string.h"
#include "subsystems/oonf_stream_socket.h"

/* built in parameters for header parser */
enum {
  OONF_HTTP_MAX_HEADERS = 16,
  OONF_HTTP_MAX_PARAMS  = 8,
  OONF_HTTP_MAX_URI_LENGTH = 256
};

enum oonf_http_result {
  HTTP_200_OK = 200,
  HTTP_400_BAD_REQ = 400,
  HTTP_401_UNAUTHORIZED = 401,
  HTTP_403_FORBIDDEN = STREAM_REQUEST_FORBIDDEN,
  HTTP_404_NOT_FOUND = 404,
  HTTP_413_REQUEST_TOO_LARGE = STREAM_REQUEST_TOO_LARGE,
  HTTP_500_INTERNAL_SERVER_ERROR = 500,
  HTTP_501_NOT_IMPLEMENTED = 501,
  HTTP_503_SERVICE_UNAVAILABLE = STREAM_SERVICE_UNAVAILABLE,
};

struct oonf_http_session {
  /* address of remote client */
  struct netaddr *remote;

  const char *method; /* get/post/... */
  const char *request_uri;
  const char *http_version;

  char *header_name[OONF_HTTP_MAX_HEADERS];
  char *header_value[OONF_HTTP_MAX_HEADERS];
  size_t header_count;

  /* parameter of the URI for GET/POST */
  char *param_name[OONF_HTTP_MAX_PARAMS];
  char *param_value[OONF_HTTP_MAX_PARAMS];
  size_t param_count;

  /* content type for answer, NULL means plain/html */
  const char *content_type;
};

struct oonf_http_handler {
  struct avl_node node;

  /* path of filename of content */
  const char *site;

  /* set by oonf_http_add to true if site is a directory */
  bool directory;

  /* list of base64 encoded name:password combinations */
  struct strarray auth;

  /* list of IP addresses/ranges this site can be accessed from */
  struct netaddr_acl acl;

  /* pointer to static content and length in bytes */
  const char *content;
  size_t content_size;

  /* callback for custom generated content (called if content==NULL) */
  enum oonf_http_result (*content_handler)(
      struct autobuf *out, struct oonf_http_session *);
};

#define LOG_HTTP oonf_http_subsystem.logging
EXPORT extern struct oonf_subsystem oonf_http_subsystem;

EXPORT extern const char *HTTP_CONTENTTYPE_HTML;
EXPORT extern const char *HTTP_CONTENTTYPE_TEXT;

EXPORT void oonf_http_add(struct oonf_http_handler *);
EXPORT void oonf_http_remove(struct oonf_http_handler *);

EXPORT const char *oonf_http_lookup_value(char **keys, char **values,
    size_t count, const char *key);

/**
 * Lookup the value of one http header field.
 * @param session pointer to http session
 * @param key header field name
 * @return header field value or NULL if not found
 */
static INLINE const char *
oonf_http_lookup_header(struct oonf_http_session *session, const char *key) {
  return oonf_http_lookup_value(session->header_name, session->header_value,
      session->header_count, key);
}

/**
 * Lookup the value of one http request parameter delivered by GET
 * @param session pointer to http session
 * @param key header field name
 * @return parameter value or NULL if not found
 */
static INLINE const char *
oonf_http_lookup_param(struct oonf_http_session *session, const char *key) {
  return oonf_http_lookup_value(session->param_name, session->param_value,
      session->param_count, key);
}

#endif /* OONF_HTTP_H_ */

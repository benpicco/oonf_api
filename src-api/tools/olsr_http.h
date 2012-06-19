
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

#ifndef OLSR_HTTP_H_
#define OLSR_HTTP_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "common/string.h"

#include "core/olsr_stream_socket.h"
#include "core/olsr_netaddr_acl.h"

/* built in parameters for header parser */
enum {
  OLSR_HTTP_MAX_HEADERS = 16,
  OLSR_HTTP_MAX_PARAMS  = 8,
  OLSR_HTTP_MAX_URI_LENGTH = 256
};

enum olsr_http_result {
  HTTP_200_OK = 200,
  HTTP_400_BAD_REQ = 400,
  HTTP_401_UNAUTHORIZED = 401,
  HTTP_403_FORBIDDEN = STREAM_REQUEST_FORBIDDEN,
  HTTP_404_NOT_FOUND = 404,
  HTTP_413_REQUEST_TOO_LARGE = STREAM_REQUEST_TOO_LARGE,
  HTTP_501_NOT_IMPLEMENTED = 501,
  HTTP_503_SERVICE_UNAVAILABLE = STREAM_SERVICE_UNAVAILABLE,
};

struct olsr_http_session {
  /* address of remote client */
  struct netaddr *remote;

  const char *method; /* get/post/... */
  const char *request_uri;
  const char *http_version;

  char *header_name[OLSR_HTTP_MAX_HEADERS];
  char *header_value[OLSR_HTTP_MAX_HEADERS];
  size_t header_count;

  /* parameter of the URI for GET/POST */
  char *param_name[OLSR_HTTP_MAX_PARAMS];
  char *param_value[OLSR_HTTP_MAX_PARAMS];
  size_t param_count;

  /* content type for answer, NULL means plain/html */
  const char *content_type;
};

struct olsr_http_handler {
  struct avl_node node;

  /* path of filename of content */
  const char *site;

  /* set by olsr_http_add to true if site is a directory */
  bool directory;

  /* list of base64 encoded name:password combinations */
  struct strarray auth;

  /* list of IP addresses/ranges this site can be accessed from */
  struct olsr_netaddr_acl acl;

  /* pointer to static content and length in bytes */
  const char *content;
  size_t content_size;

  /* callback for custom generated content (called if content==NULL) */
  enum olsr_http_result (*content_handler)(
      struct autobuf *out, struct olsr_http_session *);
};

EXPORT extern const char *HTTP_CONTENTTYPE_HTML;
EXPORT extern const char *HTTP_CONTENTTYPE_TEXT;

EXPORT void olsr_http_init(void);
EXPORT void olsr_http_cleanup(void);

EXPORT void olsr_http_add(struct olsr_http_handler *);
EXPORT void olsr_http_remove(struct olsr_http_handler *);

EXPORT const char *olsr_http_lookup_value(char **keys, char **values,
    size_t count, const char *key);

/**
 * Lookup the value of one http header field.
 * @param session pointer to http session
 * @param key header field name
 * @return header field value or NULL if not found
 */
static INLINE const char *
olsr_http_lookup_header(struct olsr_http_session *session, const char *key) {
  return olsr_http_lookup_value(session->header_name, session->header_value,
      session->header_count, key);
}

/**
 * Lookup the value of one http request parameter delivered by GET
 * @param session pointer to http session
 * @param key header field name
 * @return parameter value or NULL if not found
 */
static INLINE const char *
olsr_http_lookup_param(struct olsr_http_session *session, const char *key) {
  return olsr_http_lookup_value(session->param_name, session->param_value,
      session->param_count, key);
}

#endif /* OLSR_HTTP_H_ */

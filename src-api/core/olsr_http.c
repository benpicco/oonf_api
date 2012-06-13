
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

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"

#include "config/cfg_schema.h"

#include "core/olsr.h"
#include "core/olsr_cfg.h"
#include "core/olsr_libdata.h"
#include "core/olsr_logging.h"
#include "core/olsr_netaddr_acl.h"
#include "core/olsr_stream_socket.h"
#include "core/os_system.h"
#include "core/olsr_http.h"

/* Http text constants */
static const char HTTP_VERSION_1_0[] = "HTTP/1.0";
static const char HTTP_VERSION_1_1[] = "HTTP/1.1";

const char *HTTP_CONTENTTYPE_HTML = "text/html";
const char *HTTP_CONTENTTYPE_TEXT = "text/plain";

static const char HTTP_GET[] = "GET";
static const char HTTP_POST[] = "POST";

static const char HTTP_CONTENT_LENGTH[] = "Content-Length";

static const char HTTP_RESPONSE_200[] = "OK";
static const char HTTP_RESPONSE_400[] = "Bad Request";
static const char HTTP_RESPONSE_401[] = "Unauthorized";
static const char HTTP_RESPONSE_403[] = "Forbidden";
static const char HTTP_RESPONSE_404[] = "Not Found";
static const char HTTP_RESPONSE_413[] = "Request Entity Too Large";
static const char HTTP_RESPONSE_500[] = "Internal Server Error";
static const char HTTP_RESPONSE_501[] = "Not Implemented";
static const char HTTP_RESPONSE_503[] = "Service Unavailable";

/* static function prototypes */
static void _cb_config_changed(void);
static enum olsr_stream_session_state _cb_receive_data(
    struct olsr_stream_session *session);
static void _cb_create_error(struct olsr_stream_session *session,
    enum olsr_stream_errors error);
static bool _auth_okay(struct olsr_http_handler *handler,
    struct olsr_http_session *session);
static void _create_http_error(struct olsr_stream_session *session,
    enum olsr_http_result error);
static struct olsr_http_handler *_get_site_handler(const char *uri);
static const char *_get_headertype_string(enum olsr_http_result type);
static void _create_http_header(struct olsr_stream_session *session,
    enum olsr_http_result code, const char *content_type);
static int _parse_http_header(char *header_data, size_t header_len,
    struct olsr_http_session *header);
static size_t _parse_query_string(char *s,
    char **name, char **value, size_t count);
static void  _decode_uri(char *src);

/* configuration variables */
static struct cfg_schema_section _http_section = {
  .type = "http",
  .mode = CFG_SSMODE_UNNAMED_OPTIONAL_STARTUP_TRIGGER,
  .help = "Settings for the http interface",
  .cb_delta_handler = _cb_config_changed
};

static struct cfg_schema_entry _http_entries[] = {
  CFG_MAP_ACL_V46(olsr_stream_managed_config,
      acl, "acl", "127.0.0.1", "Access control list for http interface"),
  CFG_MAP_NETADDR_V4(olsr_stream_managed_config,
      bindto_v4, "bindto_v4", "127.0.0.1", "Bind http ipv4 socket to this address", false, true),
  CFG_MAP_NETADDR_V6(olsr_stream_managed_config,
      bindto_v6, "bindto_v6", "::1", "Bind http ipv6 socket to this address", false, true),
  CFG_MAP_INT_MINMAX(olsr_stream_managed_config,
      port, "port", "1978", "Network port for http interface", 1, 65535),
};

/* tree of http sites */
static struct avl_tree _http_site_tree;

/* http session handling */
static struct olsr_stream_managed _http_managed_socket;

/* remember if initialized or not */
OLSR_SUBSYSTEM_STATE(_http_state);

/**
 * Initialize http subsystem
 */
void
olsr_http_init(void) {
  if (olsr_subsystem_init(&_http_state))
    return;

  cfg_schema_add_section(olsr_cfg_get_schema(), &_http_section,
      _http_entries, ARRAYSIZE(_http_entries));

  olsr_stream_add_managed(&_http_managed_socket);
  _http_managed_socket.config.session_timeout = 120000; /* 120 seconds */
  _http_managed_socket.config.maximum_input_buffer = 65536;
  _http_managed_socket.config.allowed_sessions = 3;
  _http_managed_socket.config.receive_data = _cb_receive_data;
  _http_managed_socket.config.create_error = _cb_create_error;

  avl_init(&_http_site_tree, avl_comp_strcasecmp, false, NULL);
}

/**
 * Free all resources allocated by http subsystem
 */
void
olsr_http_cleanup(void) {
  if (olsr_subsystem_cleanup(&_http_state))
    return;

  olsr_stream_remove_managed(&_http_managed_socket, true);

  cfg_schema_remove_section(olsr_cfg_get_schema(), &_http_section);
}

/**
 * Add a http handler to the server. The site variable has
 * to be initialized before this call.
 * @param handler pointer to http handler
 */
void
olsr_http_add(struct olsr_http_handler *handler) {
  assert(handler->site && handler->site[0] == '/');

  handler->directory = handler->site[strlen(handler->site)-1] == '/';
  handler->node.key = handler->site;
  avl_insert(&_http_site_tree, &handler->node);
}

/**
 * Removes a http handler from the server
 * @param handler pointer to http handler
 */
void
olsr_http_remove(struct olsr_http_handler *handler) {
  avl_remove(&_http_site_tree, &handler->node);
}

/**
 * Helper function to look for a http header, get or post value
 * corresponding to a certain key.
 * Use the olsr_http_lookup_(get|post|header)() inline functions.
 *
 * @param keys pointer to list of strings (char pointers) with keys
 * @param values pointer to list of strings (char pointers) with values
 * @param count number of keys/values
 * @param key pointer to key string to look for
 * @return pointer to value or NULL if not found
 */
const char *
olsr_http_lookup_value(char **keys, char **values, size_t count, const char *key) {
  size_t i;

  for (i=0; i<count; i++) {
    if (strcmp(keys[i], key) == 0) {
      return values[i];
    }
  }
  return NULL;
}

/**
 * Callback for configuration changes
 */
static void
_cb_config_changed(void) {
  struct olsr_stream_managed_config config;

  /* generate binary config */
  memset(&config, 0, sizeof(config));
  if (cfg_schema_tobin(&config, _http_section.post,
      _http_entries, ARRAYSIZE(_http_entries))) {
    /* error in conversion */
    OLSR_WARN(LOG_HTTP, "Cannot map http config to binary data");
    goto apply_config_failed;
  }

  if (olsr_stream_apply_managed(&_http_managed_socket, &config)) {
    /* error while updating sockets */
    goto apply_config_failed;
  }

  /* fall through */
apply_config_failed:
  olsr_acl_remove(&config.acl);
}

/**
 * Callback for incoming http data
 * @param session pointer to tcp session
 * @return state of tcp session
 */
static enum olsr_stream_session_state
_cb_receive_data(struct olsr_stream_session *session) {
  struct olsr_http_session header;
  struct olsr_http_handler *handler;
  char uri[OLSR_HTTP_MAX_URI_LENGTH+1];
  char *first_header;
  char *ptr;
  size_t len;

  /* search for end of http header */
  if ((first_header = strstr(abuf_getptr(&session->in), "\r\n\r\n"))) {
    first_header += 4;
  }
  else if ((first_header = strstr(abuf_getptr(&session->in), "\n\n"))) {
    first_header += 2;
  }
  else {
    /* still waiting for end of http header */
    return STREAM_SESSION_ACTIVE;
  }

  if (_parse_http_header(abuf_getptr(&session->in), abuf_getlen(&session->in), &header)) {
    OLSR_INFO(LOG_HTTP, "Error, malformed HTTP header.\n");
    _create_http_error(session, HTTP_400_BAD_REQ);
    return STREAM_SESSION_SEND_AND_QUIT;
  }

  if (strcmp(header.http_version, HTTP_VERSION_1_0) != 0
      && strcmp(header.http_version, HTTP_VERSION_1_1) != 0) {
    OLSR_INFO(LOG_HTTP, "Unknown HTTP version: '%s'\n", header.http_version);
    _create_http_error(session, HTTP_400_BAD_REQ);
    return STREAM_SESSION_SEND_AND_QUIT;
  }

  len = strlen(header.request_uri);
  if (len >= OLSR_HTTP_MAX_URI_LENGTH) {
    OLSR_INFO(LOG_HTTP, "Too long URI in HTTP header: '%s'\n", header.request_uri);
    _create_http_error(session, HTTP_400_BAD_REQ);
    return STREAM_SESSION_SEND_AND_QUIT;
  }

  OLSR_DEBUG(LOG_HTTP, "Incoming HTTP request: %s %s %s\n",
      header.method, header.request_uri, header.http_version);

  /* make working copy of URI string */
  strscpy(uri, header.request_uri, sizeof(uri));

  if (strcmp(header.method, HTTP_POST) == 0) {
    const char *content_length;

    content_length = olsr_http_lookup_value(header.header_name, header.header_value,
        header.header_count, HTTP_CONTENT_LENGTH);
    if (!content_length) {
      OLSR_INFO(LOG_HTTP, "Need 'content-length' for POST requests");
      _create_http_error(session, HTTP_400_BAD_REQ);
      return STREAM_SESSION_SEND_AND_QUIT;
    }

    if (strtoul(content_length, NULL, 10) > abuf_getlen(&session->in)) {
      /* header not complete */
      return STREAM_SESSION_ACTIVE;;
    }

    header.param_count = _parse_query_string(first_header,
        header.param_name, header.param_value, OLSR_HTTP_MAX_PARAMS);
  }

  /* strip the URL fragment away */
  ptr = strchr(uri, '#');
  if (ptr) {
    *ptr = 0;
  }

  /* decode special characters of URI */
  _decode_uri(uri);

  if (strcmp(header.method, HTTP_GET) == 0) {
    /* HTTP-GET request */
    ptr = strchr(uri, '?');
    if (ptr != NULL) {
      *ptr++ = 0;
      header.param_count = _parse_query_string(ptr,
          header.param_name, header.param_value, OLSR_HTTP_MAX_PARAMS);
    }
  } else if (strcmp(header.method, HTTP_POST) != 0) {
    OLSR_INFO(LOG_HTTP, "HTTP method not implemented :'%s'", header.method);
    _create_http_error(session, HTTP_501_NOT_IMPLEMENTED);
    return STREAM_SESSION_SEND_AND_QUIT;
  }

  handler = _get_site_handler(uri);
  if (handler == NULL) {
    OLSR_DEBUG(LOG_HTTP, "No HTTP handler for site: %s", uri);
    _create_http_error(session, HTTP_404_NOT_FOUND);
    return STREAM_SESSION_SEND_AND_QUIT;
  }

  if (handler->content) {
    abuf_memcpy(&session->out, handler->content, handler->content_size);
    _create_http_header(session, HTTP_200_OK, NULL);
  }
  else {
    enum olsr_http_result result;

    /* check acl */
    if (!olsr_acl_check_accept(&handler->acl, &session->remote_address)) {
      _create_http_error(session, HTTP_403_FORBIDDEN);
      return STREAM_SESSION_SEND_AND_QUIT;
    }

    /* check if username/password is necessary */
    if (!strarray_is_empty(&handler->auth)) {
      if (!_auth_okay(handler, &header)) {
        _create_http_error(session, HTTP_401_UNAUTHORIZED);
        return STREAM_SESSION_SEND_AND_QUIT;
      }
    }

    result = handler->content_handler(&session->out, &header);
    if (result != HTTP_200_OK) {
      /* create error message */
      _create_http_error(session, result);
    }
    else {
      _create_http_header(session, HTTP_200_OK, header.content_type);
    }
  }
  return STREAM_SESSION_SEND_AND_QUIT;
}

/**
 * Check if an incoming session is authorized to view a http site.
 * @param handler pointer to site handler
 * @param session pointer to http session.
 * @return true if authorized, false if not
 */
static bool
_auth_okay(struct olsr_http_handler *handler,
    struct olsr_http_session *session) {
  const char *auth, *name_pw_base64;
  char *ptr;

  auth = olsr_http_lookup_header(session, "Authorization");
  if (auth == NULL) {
    return false;
  }

  name_pw_base64 = str_hasnextword(auth, "Basic");
  if (name_pw_base64 == NULL) {
    return false;
  }

  FOR_ALL_STRINGS(&handler->auth, ptr) {
    if (strcmp(ptr, name_pw_base64) == 0) {
      return true;
    }
  }
  return false;
}

/**
 * Callback for generating a TCP error
 * @param session pointer to tcp session
 * @param error tcp error code
 */
static void
_cb_create_error(struct olsr_stream_session *session,
    enum olsr_stream_errors error) {
  _create_http_error(session, (enum olsr_http_result)error);
}

/**
 * Create body and header for a HTTP error
 * @param session pointer to tcp session
 * @param error http error code
 */
static void
_create_http_error(struct olsr_stream_session *session,
    enum olsr_http_result error) {
  abuf_appendf(&session->out, "<html><head><title>%s %s http server</title></head>"
      "<body><h1>HTTP error %d: %s</h1></body></html>",
      olsr_log_get_appdata()->app_name, olsr_log_get_appdata()->app_version,
      error, _get_headertype_string(error));
  _create_http_header(session, error, NULL);
}

/**
 * Lookup the http site handler for an URI
 * @param uri pointer to URI
 * @return http site handler or NULL if none available
 */
static struct olsr_http_handler *
_get_site_handler(const char *uri) {
  struct olsr_http_handler *handler;
  size_t len;

  /* look for exact match */
  handler = avl_find_element(&_http_site_tree, uri, handler, node);
  if (handler) {
    return handler;
  }

  /* look for directory handler with shorter URL */
  handler = avl_find_le_element(&_http_site_tree, uri, handler, node);
  if (handler && handler->directory) {
    len = strlen(handler->site);

    /* check if complete handler path (ending with /) matchs uri */
    if (strncasecmp(handler->site, uri, len) == 0) {
      return handler;
    }
  }

  /* user might have skipped trailing / for directory */
  handler = avl_find_ge_element(&_http_site_tree, uri, handler, node);
  if (handler) {
    len = strlen(uri);

    if (strncasecmp(handler->site, uri, len) == 0
        && handler->site[len] == '/' && handler->site[len+1] == 0) {
      return handler;
    }
  }
  return NULL;
}

/**
 * @param type http result code
 * @return string representation of http result code
 */
static const char *
_get_headertype_string(enum olsr_http_result type) {
  switch (type) {
    case HTTP_200_OK:
      return HTTP_RESPONSE_200;
    case HTTP_400_BAD_REQ:
      return HTTP_RESPONSE_400;
    case HTTP_401_UNAUTHORIZED:
      return HTTP_RESPONSE_401;
    case HTTP_403_FORBIDDEN:
      return HTTP_RESPONSE_403;
    case HTTP_404_NOT_FOUND:
      return HTTP_RESPONSE_404;
    case HTTP_413_REQUEST_TOO_LARGE:
      return HTTP_RESPONSE_413;
    case HTTP_501_NOT_IMPLEMENTED:
      return HTTP_RESPONSE_501;
    case HTTP_503_SERVICE_UNAVAILABLE:
      return HTTP_RESPONSE_503;
    default:
      return HTTP_RESPONSE_500;
  }
}

/**
 * Create a http header for an existing content and put
 * it in front of the content.
 * @param session pointer to tcp session
 * @param code http result code
 * @param content_type explicit content type or NULL for
 *   plain html
 */
static void
_create_http_header(struct olsr_stream_session *session,
    enum olsr_http_result code, const char *content_type) {
  struct autobuf buf;
  struct timeval currtime;

  abuf_init(&buf);

  abuf_appendf(&buf, "%s %d %s\r\n", HTTP_VERSION_1_0, code, _get_headertype_string(code));

  /* Date */
  os_system_gettimeofday(&currtime);
  abuf_strftime(&buf, "Date: %a, %d %b %Y %H:%M:%S GMT\r\n", localtime(&currtime.tv_sec));

  /* Server version */
  abuf_appendf(&buf, "Server: %s %s %s\r\n",
      olsr_log_get_appdata()->app_version,
      olsr_log_get_appdata()->builddate,
      olsr_log_get_appdata()->buildsystem);

  /* connection-type */
  abuf_puts(&buf, "Connection: closed\r\n");

  /* MIME type */
  if (content_type == NULL) {
    content_type = HTTP_CONTENTTYPE_HTML;
  }
  abuf_appendf(&buf, "Content-type: %s\r\n", content_type);

  /* Content length */
  if (abuf_getlen(&session->out) > 0) {
    abuf_appendf(&buf, "Content-length: %zu\r\n", abuf_getlen(&session->out));
  }

  if (code == HTTP_401_UNAUTHORIZED) {
    abuf_appendf(&buf, "WWW-Authenticate: Basic realm=\"%s\"\r\n", "RealmName");
  }

  /*
   * Cache-control
   * No caching dynamic pages
   */
  abuf_puts(&buf, "Cache-Control: no-cache\r\n");

  /* End header */
  abuf_puts(&buf, "\r\n");

  abuf_memcpy_prepend(&session->out, abuf_getptr(&buf), abuf_getlen(&buf));
  OLSR_DEBUG(LOG_HTTP, "Generated Http-Header:\n%s", abuf_getptr(&buf));

  abuf_free(&buf);
}

/**
 * Parse a HTTP header
 * @param header_data pointer to header data
 * @param header_len length of header data
 * @param header pointer to object to store the results
 * @return 0 if http header was correct, -1 if an error happened
 */
static int
_parse_http_header(char *header_data, size_t header_len,
    struct olsr_http_session *header) {
  size_t header_index;

  assert(header_data);
  assert(header);

  memset(header, 0, sizeof(struct olsr_http_session));
  header->method = header_data;

  while(true) {
    if (header_len < 2) {
      goto unexpected_end;
    }

    if (*header_data == ' ' && header->http_version == NULL) {
      *header_data = '\0';

      if (header->request_uri == NULL) {
        header->request_uri = &header_data[1];
      }
      else if (header->http_version == NULL) {
        header->http_version = &header_data[1];
      }
    }
    else if (*header_data == '\r') {
      *header_data = '\0';
    }
    else if (*header_data == '\n') {
      *header_data = '\0';

      header_data++; header_len--;
      break;
    }

    header_data++; header_len--;
  }

  if (header->http_version == NULL) {
    goto unexpected_end;
  }

  for(header_index = 0; true; header_index++) {
    if (header_len < 1) {
      goto unexpected_end;
    }

    if (*header_data == '\n') {
      break;
    }
    else if (*header_data == '\r') {
      if (header_len < 2) return true;

      if (header_data[1] == '\n') {
        break;
      }
    }

    if (header_index >= OLSR_HTTP_MAX_HEADERS) {
      goto too_many_fields;
    }

    header->header_name[header_index] = header_data;

    while(true) {
      if (header_len < 1) {
        goto unexpected_end;
      }

      if (*header_data == ':') {
        *header_data = '\0';

        header_data++; header_len--;
        break;
      }
      else if (*header_data == ' ' || *header_data == '\t') {
        *header_data = '\0';
      }
      else if (*header_data == '\n' || *header_data == '\r') {
        goto unexpected_end;
      }

      header_data++; header_len--;
    }

    while(true) {
      if (header_len < 1) {
        goto unexpected_end;
      }

      if (header->header_value[header_index] == NULL) {
        if (*header_data != ' ' && *header_data != '\t') {
          header->header_value[header_index] = header_data;
        }
      }

      if (*header_data == '\n') {
        if (header_len < 2) {
          goto unexpected_end;
        }

        if (header_data[1] == ' ' || header_data[1] == '\t') {
          *header_data = ' ';
          header_data[1] = ' ';

          header_data += 2; header_len -= 2;
          continue;
        }

        *header_data = '\0';

        if (header->header_value[header_index] == NULL) {
          header->header_value[header_index] = header_data;
        }

        header_data++; header_len--;
        break;
      }
      else if (*header_data == '\r') {
        if (header_len < 2) {
          goto unexpected_end;
        }

        if (header_data[1] == '\n') {
          if (header_len < 3) {
            goto unexpected_end;
          }

          if (header_data[2] == ' ' || header_data[2] == '\t') {
            *header_data = ' ';
            header_data[1] = ' ';
            header_data[2] = ' ';

            header_data += 3; header_len -= 3;
            continue;
          }

          *header_data = '\0';

          if (header->header_value[header_index] == NULL) {
            header->header_value[header_index] = header_data;
          }

          header_data += 2; header_len -= 2;
          break;
        }
      }

      header_data++; header_len--;
    }
  }

  header->header_count = header_index;
  return 0;

too_many_fields:
  OLSR_DEBUG(LOG_HTTP, "Error, too many HTTP header fields\n");
  return -1;

unexpected_end:
  OLSR_DEBUG(LOG_HTTP, "Error, unexpected end of HTTP header\n");
  return -1;
}

/**
 * Parse the query string (either get or post) and store it into
 * a list of key/value pointers. The original string will be
 * modified for doing this.
 * @param s pointer to query string
 * @param name pointer to array of stringpointers for keys
 * @param value pointer to array of stringpointers for values
 * @param count maximum allowed number of keys/values
 * @return number of generated keys/values
 */
static size_t
_parse_query_string(char *s, char **name, char **value, size_t count) {
  char *ptr;
  size_t i = 0;

  assert(s);
  assert(name);
  assert(value);

  while (s != NULL && i < count) {
    name[i] = s;

    s = strchr(s, '&');
    if (s != NULL) {
      *s++ = '\0';
    }

    ptr = strchr(name[i], '=');
    if (ptr != NULL) {
      *ptr++ = '\0';
      value[i] = ptr;
    } else {
      value[i] = &name[i][strlen(name[i])];
    }

    if(name[i][0] != '\0') {
      i++;
    }
  }

  return i;
}

/**
 * Decode encoded characters of an URI. The URL will be modified
 * inline by this function.
 * @param src pointer to URI string
 */
static void
_decode_uri(char *src) {
  char *dst = src;

  while (*src) {
    if (*src == '%' && src[1] && src[2]) {
      int value = 0;

      src++;
      sscanf(src, "%02x", &value);
      *dst++ = (char) value;
      src += 2;
    } else {
      *dst++ = *src++;
    }
  }
  *dst = 0;
}

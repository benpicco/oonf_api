
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

#include <string.h>
#include <stdlib.h>
#include <errno.h>

#include "common/autobuf.h"
#include "common/avl.h"
#include "common/list.h"
#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_socket.h"
#include "subsystems/oonf_timer.h"
#include "subsystems/os_net.h"
#include "subsystems/oonf_stream_socket.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static int _apply_managed_socket(struct oonf_stream_managed *managed,
    struct oonf_stream_socket *stream, struct netaddr *bindto, uint16_t port);
static void _cb_parse_request(int fd, void *data, bool, bool);
static struct oonf_stream_session *_create_session(
    struct oonf_stream_socket *stream_socket, int sock, struct netaddr *remote_addr);
static void _cb_parse_connection(int fd, void *data, bool r,bool w);

static void _cb_timeout_handler(void *);

/* list of olsr stream sockets */
struct list_entity oonf_stream_head;

/* server socket */
static struct oonf_class _connection_cookie = {
  .name = "stream socket connection",
  .size = sizeof(struct oonf_stream_session)
};

static struct oonf_timer_info _connection_timeout = {
  .name = "stream socket timout",
  .callback = _cb_timeout_handler,
};

/* subsystem definition */
struct oonf_subsystem oonf_stream_socket_subsystem = {
  .name = "stream",
  .init = _init,
  .cleanup = _cleanup,
};

/**
 * Initialize the stream socket handlers
 * @return always returns 0
 */
static int
_init(void) {
  oonf_class_add(&_connection_cookie);
  oonf_timer_add(&_connection_timeout);
  list_init_head(&oonf_stream_head);
  return 0;
}

/**
 * Cleanup all resources allocated be stream socket handlers
 */
static void
_cleanup(void) {
  struct oonf_stream_socket *comport;

  while (!list_is_empty(&oonf_stream_head)) {
    comport = list_first_element(&oonf_stream_head, comport, node);

    oonf_stream_remove(comport, true);
  }

  oonf_class_remove(&_connection_cookie);
  oonf_timer_remove(&_connection_timeout);
}

/**
 * Flush all data in outgoing buffer of a stream socket
 * @param con pointer to stream socket
 */
void
oonf_stream_flush(struct oonf_stream_session *con) {
  oonf_socket_set_write(&con->scheduler_entry, true);
}

/**
 * Add a new stream socket to the scheduler
 * @param stream_socket pointer to uninitialized stream socket struct
 * @param local pointer to local ip/port of socket, port must be 0 if
 *   this shall be an outgoing socket
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_stream_add(struct oonf_stream_socket *stream_socket,
    const union netaddr_socket *local) {
  int s = -1;
  struct netaddr_str buf;

  memset(stream_socket, 0, sizeof(*stream_socket));

  /* server socket not necessary for outgoing connections */
  if (netaddr_socket_get_port(local) != 0) {
    /* Init socket */
    s = os_net_getsocket(local, true, 0, NULL, LOG_STREAM);
    if (s < 0) {
      goto add_stream_error;
    }

    /* show that we are willing to listen */
    if (os_net_listen(s, 1) == -1) {
      OONF_WARN(LOG_STREAM, "tcp socket listen failed for %s: %s (%d)\n",
          netaddr_socket_to_string(&buf, local), strerror(errno), errno);
      goto add_stream_error;
    }

    stream_socket->scheduler_entry.fd = s;
    stream_socket->scheduler_entry.process = _cb_parse_request;
    stream_socket->scheduler_entry.data = stream_socket;
    stream_socket->scheduler_entry.event_read = true;

    oonf_socket_add(&stream_socket->scheduler_entry);
  }
  memcpy(&stream_socket->local_socket, local, sizeof(stream_socket->local_socket));

  if (stream_socket->config.memcookie == NULL) {
    stream_socket->config.memcookie = &_connection_cookie;
  }
  if (stream_socket->config.allowed_sessions == 0) {
    stream_socket->config.allowed_sessions = 10;
  }
  if (stream_socket->config.maximum_input_buffer == 0) {
    stream_socket->config.maximum_input_buffer = 65536;
  }

  list_init_head(&stream_socket->session);
  list_add_tail(&oonf_stream_head, &stream_socket->node);

  return 0;

add_stream_error:
  if (stream_socket->scheduler_entry.fd) {
    oonf_socket_remove(&stream_socket->scheduler_entry);
  }
  if (s != -1) {
    os_net_close(s);
  }
  return -1;
}

/**
 * Remove a stream socket from the scheduler
 * @param stream_socket pointer to socket
 * @param force true if socket will be closed immediately,
 *   false if scheduler should wait until outgoing buffers are empty
 */
void
oonf_stream_remove(struct oonf_stream_socket *stream_socket, bool force) {
  struct oonf_stream_session *session, *ptr;

  if (stream_socket->busy && !force) {
    stream_socket->remove = true;
    return;
  }

  if (!list_is_node_added(&stream_socket->node)) {
    return;
  }

  list_for_each_element_safe(&stream_socket->session, session, node, ptr) {
    if (force || (abuf_getlen(&session->out) == 0 && !session->busy)) {
      /* close everything that doesn't need to send data anymore */
      oonf_stream_close(session, force);
    }
  }

  if (!list_is_empty(&stream_socket->session)) {
    return;
  }

  list_remove(&stream_socket->node);

  if (stream_socket->scheduler_entry.fd) {
    /* only for server sockets */
    os_net_close(stream_socket->scheduler_entry.fd);
    oonf_socket_remove(&stream_socket->scheduler_entry);
  }
}

/**
 * Create an outgoing stream socket.
 * @param stream_socket pointer to stream socket
 * @param remote pointer to address of remote TCP server
 * @return pointer to stream session, NULL if an error happened.
 */
struct oonf_stream_session *
oonf_stream_connect_to(struct oonf_stream_socket *stream_socket,
    const union netaddr_socket *remote) {
  struct oonf_stream_session *session;
  struct netaddr remote_addr;
  bool wait_for_connect = false;
  int s;
  struct netaddr_str buf;

  s = os_net_getsocket(&stream_socket->local_socket,
      true, 0, NULL, LOG_STREAM);
  if (s < 0) {
    return NULL;
  }

  if (os_net_connect(s, remote)) {
    if (errno != EINPROGRESS) {
      OONF_WARN(LOG_STREAM, "Cannot connect outgoing tcp connection to %s: %s (%d)",
          netaddr_socket_to_string(&buf, remote), strerror(errno), errno);
      goto connect_to_error;
    }
    wait_for_connect = true;
  }

  netaddr_from_socket(&remote_addr, remote);
  session = _create_session(stream_socket, s, &remote_addr);
  if (session) {
    session->wait_for_connect = wait_for_connect;
    return session;
  }

  /* fall through */
connect_to_error:
  if (s) {
    os_net_close(s);
  }
  return NULL;
}

/**
 * Reset the session timeout of a TCP session
 * @param con pointer to stream session
 * @param timeout timeout in milliseconds
 */
void
oonf_stream_set_timeout(struct oonf_stream_session *con, uint64_t timeout) {
  oonf_timer_set(&con->timeout, timeout);
}

/**
 * Close a TCP stream session
 * @param session pointer to stream session
 * @param force true if the session should be closed instantly,
 *   false if all data in queue should still be sent
 */
void
oonf_stream_close(struct oonf_stream_session *session, bool force) {
  if (session->busy && !force) {
    /* remove the session later */
    session->removed = true;
    return;
  }

  if (!list_is_node_added(&session->node)) {
    return;
  }

  if (session->comport->config.cleanup) {
    session->comport->config.cleanup(session);
  }

  oonf_timer_stop(&session->timeout);

  session->comport->config.allowed_sessions++;
  list_remove(&session->node);

  os_net_close(session->scheduler_entry.fd);
  oonf_socket_remove(&session->scheduler_entry);

  abuf_free(&session->in);
  abuf_free(&session->out);

  oonf_class_free(session->comport->config.memcookie, session);
}

/**
 * Initialized a managed TCP stream
 * @param managed pointer to initialized managed stream
 */
void
oonf_stream_add_managed(struct oonf_stream_managed *managed) {
  if (managed->config.allowed_sessions == 0) {
    managed->config.allowed_sessions = 10;
  }
  if (managed->config.maximum_input_buffer == 0) {
    managed->config.maximum_input_buffer = 65536;
  }
  if (managed->config.session_timeout == 0) {
    managed->config.session_timeout = 120000;
  }
}

/**
 * Apply a configuration to a stream. Will reset both ACLs
 * and socket ports/bindings.
 * @param managed pointer to managed stream
 * @param config pointer to stream config
 * @return -1 if an error happened, 0 otherwise.
 */
int
oonf_stream_apply_managed(struct oonf_stream_managed *managed,
    struct oonf_stream_managed_config *config) {
  netaddr_acl_copy(&managed->acl, &config->acl);

  if (_apply_managed_socket(managed,
      &managed->socket_v4, &config->bindto_v4, config->port)) {
    return -1;
  }

  if (os_net_is_ipv6_supported()) {
    if (_apply_managed_socket(managed,
        &managed->socket_v6, &config->bindto_v6, config->port)) {
      return -1;
    }
  }
  return 0;
}

/**
 * Remove a managed TCP stream
 * @param managed pointer to managed stream
 * @param force true if socket will be closed immediately,
 *   false if scheduler should wait until outgoing buffers are empty
 */
void
oonf_stream_remove_managed(struct oonf_stream_managed *managed, bool force) {
  oonf_stream_remove(&managed->socket_v4, force);
  oonf_stream_remove(&managed->socket_v6, force);

  netaddr_acl_remove(&managed->acl);
}

/**
 * Apply new configuration to a managed stream socket
 * @param managed pointer to managed stream
 * @param stream pointer to TCP stream to configure
 * @param bindto local address to bind socket to
 * @param port local port number
 * @return -1 if an error happened, 0 otherwise.
 */
static int
_apply_managed_socket(struct oonf_stream_managed *managed,
    struct oonf_stream_socket *stream,
    struct netaddr *bindto, uint16_t port) {
  union netaddr_socket sock;
  struct netaddr_str buf;

  if (netaddr_get_address_family(bindto) == AF_UNSPEC) {
    oonf_stream_remove(stream, true);
    return 0;
  }
  if (netaddr_socket_init(&sock, bindto, port, 0)) {
    OONF_WARN(LOG_STREAM, "Cannot create managed socket address: %s/%u",
        netaddr_to_string(&buf, bindto), port);
    return -1;
  }

  if (list_is_node_added(&stream->scheduler_entry._node)) {
    if (memcmp(&sock, &stream->local_socket, sizeof(sock)) == 0) {
      /* nothing changed */
      return 0;
    }

    oonf_stream_remove(stream, true);
  }
  if (oonf_stream_add(stream, &sock)) {
    return -1;
  }

  /* copy configuration */
  memcpy(&stream->config, &managed->config, sizeof(stream->config));
  if (stream->config.memcookie == NULL) {
    stream->config.memcookie = &_connection_cookie;
  }
  return 0;
}

/**
 * Handle incoming server socket event from socket scheduler.
 * @param fd filedescriptor for event
 * @param data custom user data
 * @param event_read true if read-event is incoming
 * @param event_write true if write-event is incoming
 */
static void
_cb_parse_request(int fd, void *data, bool event_read,
    bool event_write __attribute__((unused))) {
  struct oonf_stream_socket *comport;
  union netaddr_socket remote_socket;
  struct netaddr remote_addr;
  socklen_t addrlen;
  int sock;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf1, buf2;
#endif

  if (!event_read) {
    return;
  }

  comport = data;

  addrlen = sizeof(remote_socket);
  sock = accept(fd, &remote_socket.std, &addrlen);
  if (sock < 0) {
    OONF_WARN(LOG_STREAM, "accept() call returned error: %s (%d)", strerror(errno), errno);
    return;
  }

  netaddr_from_socket(&remote_addr, &remote_socket);
  if (comport->config.acl) {
    if (!netaddr_acl_check_accept(comport->config.acl, &remote_addr)) {
      OONF_DEBUG(LOG_STREAM, "Access from %s to socket %s blocked because of ACL",
          netaddr_to_string(&buf1, &remote_addr),
          netaddr_socket_to_string(&buf2, &comport->local_socket));
      close(sock);
      return;
    }
  }
  _create_session(comport, sock, &remote_addr);
}

/**
 * Configure a TCP session socket
 * @param stream_socket pointer to stream socket
 * @param sock pointer to socket filedescriptor
 * @param remote_addr pointer to remote address
 * @return pointer to new stream session, NULL if an error happened.
 */
static struct oonf_stream_session *
_create_session(struct oonf_stream_socket *stream_socket,
    int sock, struct netaddr *remote_addr) {
  struct oonf_stream_session *session;
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf;
#endif

  /* put socket into non-blocking mode */
  if (os_net_set_nonblocking(sock)) {
    OONF_WARN(LOG_STREAM, "Cannot read comport socket status: %s (%d)",
        strerror(errno), errno);
    return NULL;
  }

  session = oonf_class_malloc(stream_socket->config.memcookie);
  if (session == NULL) {
    OONF_WARN(LOG_STREAM, "Cannot allocate memory for comport session");
    goto parse_request_error;
  }

  if (abuf_init(&session->in)) {
    OONF_WARN(LOG_STREAM, "Cannot allocate memory for comport session");
    goto parse_request_error;
  }
  if (abuf_init(&session->out)) {
    OONF_WARN(LOG_STREAM, "Cannot allocate memory for comport session");
    goto parse_request_error;
  }

  session->scheduler_entry.fd = sock;
  session->scheduler_entry.process = _cb_parse_connection;
  session->scheduler_entry.data = session;
  session->scheduler_entry.event_read = true;
  session->scheduler_entry.event_write = true;
  oonf_socket_add(&session->scheduler_entry);

  session->send_first = stream_socket->config.send_first;
  session->comport = stream_socket;

  session->remote_address = *remote_addr;

  if (stream_socket->config.allowed_sessions-- > 0) {
    /* create active session */
    session->state = STREAM_SESSION_ACTIVE;
  } else {
    /* too many sessions */
    if (stream_socket->config.create_error) {
      stream_socket->config.create_error(session, STREAM_SERVICE_UNAVAILABLE);
    }
    session->state = STREAM_SESSION_SEND_AND_QUIT;
  }

  session->timeout.cb_context = session;
  session->timeout.info = &_connection_timeout;
  if (stream_socket->config.session_timeout) {
    oonf_timer_start(&session->timeout, stream_socket->config.session_timeout);
  }

  if (stream_socket->config.init) {
    if (stream_socket->config.init(session)) {
      goto parse_request_error;
    }
  }

  OONF_DEBUG(LOG_STREAM, "Got connection through socket %d with %s.\n",
      sock, netaddr_to_string(&buf, remote_addr));

  list_add_tail(&stream_socket->session, &session->node);
  return session;

parse_request_error:
  abuf_free(&session->in);
  abuf_free(&session->out);
  oonf_class_free(stream_socket->config.memcookie, session);

  return NULL;
}

/**
 * Handle TCP session timeout
 * @param data custom data
 */
static void
_cb_timeout_handler(void *data) {
  struct oonf_stream_session *session = data;
  oonf_stream_close(session, false);
}

/**
 * Handle events for TCP session from network scheduler
 * @param fd filedescriptor of TCP session
 * @param data custom data
 * @param event_read true if read-event is incoming
 * @param event_write true if write-event is incoming
 */
static void
_cb_parse_connection(int fd, void *data, bool event_read, bool event_write) {
  struct oonf_stream_session *session;
  struct oonf_stream_socket *s_sock;
  int len;
  char buffer[1024];
  struct netaddr_str buf;

  session = data;
  s_sock = session->comport;

  OONF_DEBUG(LOG_STREAM, "Parsing connection of socket %d\n", fd);

  /* mark session and s_sock as busy */
  session->busy = true;
  s_sock->busy = true;

  if (session->wait_for_connect) {
    if (event_write) {
      int value;

      if(os_net_get_socket_error(fd, &value)) {
        OONF_WARN(LOG_STREAM, "getsockopt failed: %s (%d)",
            strerror(errno), errno);
        session->state = STREAM_SESSION_CLEANUP;
      }
      else if (value != 0) {
        OONF_WARN(LOG_STREAM, "Connection to %s failed: %s (%d)",
            netaddr_to_string(&buf, &session->remote_address), strerror(value), value);
        session->state = STREAM_SESSION_CLEANUP;
      }
      else {
        session->wait_for_connect = false;
      }
    }
  }

  if (session->wait_for_connect) {
    session->busy = false;
    s_sock->busy = false;
    return;
  }

  /* read data if necessary */
  if (session->state == STREAM_SESSION_ACTIVE && event_read) {
    len = os_net_recvfrom(fd, buffer, sizeof(buffer), NULL, 0);
    if (len > 0) {
      OONF_DEBUG(LOG_STREAM, "  recv returned %d\n", len);
      if (abuf_memcpy(&session->in, buffer, len)) {
        /* out of memory */
        OONF_WARN(LOG_STREAM, "Out of memory for comport session input buffer");
        session->state = STREAM_SESSION_CLEANUP;
      } else if (abuf_getlen(&session->in) > s_sock->config.maximum_input_buffer) {
        /* input buffer overflow */
        if (s_sock->config.create_error) {
          s_sock->config.create_error(session, STREAM_REQUEST_TOO_LARGE);
        }
        session->state = STREAM_SESSION_SEND_AND_QUIT;
      } else {
        /* got new input block, reset timeout */
        oonf_stream_set_timeout(session, s_sock->config.session_timeout);
      }
    } else if (len < 0 && errno != EINTR && errno != EAGAIN && errno
        != EWOULDBLOCK) {
      /* error during read */
      OONF_WARN(LOG_STREAM, "Error while reading from communication stream with %s: %s (%d)\n",
          netaddr_to_string(&buf, &session->remote_address), strerror(errno), errno);
      session->state = STREAM_SESSION_CLEANUP;
    } else if (len == 0) {
      /* external s_sock closed */
      session->state = STREAM_SESSION_SEND_AND_QUIT;
    }
  }

  if (session->state == STREAM_SESSION_ACTIVE && s_sock->config.receive_data != NULL
      && (abuf_getlen(&session->in) > 0 || session->send_first)) {
    session->state = s_sock->config.receive_data(session);
    session->send_first = false;
  }

  /* send data if necessary */
  if (session->state != STREAM_SESSION_CLEANUP && abuf_getlen(&session->out) > 0) {
    if (event_write) {
      len = os_net_sendto(fd, abuf_getptr(&session->out), abuf_getlen(&session->out), NULL);

      if (len > 0) {
        OONF_DEBUG(LOG_STREAM, "  send returned %d\n", len);
        abuf_pull(&session->out, len);
        oonf_stream_set_timeout(session, s_sock->config.session_timeout);
      } else if (len < 0 && errno != EINTR && errno != EAGAIN && errno
          != EWOULDBLOCK) {
        OONF_WARN(LOG_STREAM, "Error while writing to communication stream with %s: %s (%d)\n",
            netaddr_to_string(&buf, &session->remote_address), strerror(errno), errno);
        session->state = STREAM_SESSION_CLEANUP;
      }
    } else {
      OONF_DEBUG(LOG_STREAM, "  activating output in scheduler\n");
      oonf_socket_set_write(&session->scheduler_entry, true);
    }
  }

  if (abuf_getlen(&session->out) == 0) {
    /* nothing to send anymore */
    OONF_DEBUG(LOG_STREAM, "  deactivating output in scheduler\n");
    oonf_socket_set_write(&session->scheduler_entry, false);
    if (session->state == STREAM_SESSION_SEND_AND_QUIT) {
      session->state = STREAM_SESSION_CLEANUP;
    }
  }

  session->busy = false;
  s_sock->busy = false;

  /* end of connection ? */
  if (session->state == STREAM_SESSION_CLEANUP || session->removed) {
    OONF_DEBUG(LOG_STREAM, "  cleanup\n");

    /* clean up connection by calling cleanup directly */
    oonf_stream_close(session, session->state == STREAM_SESSION_CLEANUP);
  }

  /* lazy socket removal */
  if (s_sock->remove) {
    oonf_stream_remove(s_sock, false);
  }
  return;
}

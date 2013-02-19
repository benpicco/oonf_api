
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

#ifndef OLSR_STREAM_SOCKET_H_
#define OLSR_STREAM_SOCKET_H_

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/list.h"
#include "common/netaddr.h"

#include "core/olsr_class.h"
#include "core/olsr_netaddr_acl.h"
#include "core/olsr_socket.h"
#include "core/olsr_timer.h"

enum olsr_stream_session_state {
  STREAM_SESSION_ACTIVE,
  STREAM_SESSION_SEND_AND_QUIT,
  STREAM_SESSION_CLEANUP,
};

enum olsr_stream_errors {
  STREAM_REQUEST_FORBIDDEN = 403,
  STREAM_REQUEST_TOO_LARGE = 413,
  STREAM_SERVICE_UNAVAILABLE = 503,
};

/* represents a TCP stream */
struct olsr_stream_session {
  /*
   * public part of the session data
   *
   * variables marked RW might be written from txt commands, those with
   * an "R" mark are read only
   */

  /* ip addr of peer (R) */
  struct netaddr remote_address;

  /* output buffer, anything inside will be written to the peer as
   * soon as possible */
  struct autobuf out;

  /*
   * internal part of the server
   */
  struct list_entity node;

  /* backpointer to the stream socket */
  struct olsr_stream_socket *comport;

  /* scheduler handler for the session */
  struct olsr_socket_entry scheduler_entry;

  /* timer for handling session timeout */
  struct olsr_timer_entry timeout;

  /* input buffer for session */
  struct autobuf in;

  /*
   * true if session user want to send before receiving anything. Will trigger
   * an empty read even as soon as session is connected
   */
  bool send_first;

  /* true if session is still waiting for initial handshake to finish */
  bool wait_for_connect;

  /* session event is just busy in scheduler */
  bool busy;

  /* session has been remove while being busy */
  bool removed;

  enum olsr_stream_session_state state;
};

struct olsr_stream_config {
  /* memory cookie to allocate struct for tcp session */
  struct olsr_class *memcookie;

  /* number of simultaneous sessions (default 10) */
  int allowed_sessions;

  /*
   * Timeout of the socket. A session will be closed if it does not
   * send or receive data for timeout milliseconds.
   */
  uint64_t session_timeout;

  /* maximum allowed size of input buffer (default 65536) */
  size_t maximum_input_buffer;

  /*
   * true if the socket wants to send data before it receives anything.
   * This will trigger an size 0 read event as soon as the socket is connected
   */
  bool send_first;

  /* only clients that match the acl (if set) can connect */
  struct olsr_netaddr_acl *acl;

  /* Called when a new session is created */
  int (*init)(struct olsr_stream_session *);

  /* Called when a TCP session ends */
  void (*cleanup)(struct olsr_stream_session *);

  /*
   * An error happened during parsing the TCP session,
   * the user of the session might want to create an error message
   */
  void (*create_error)(struct olsr_stream_session *, enum olsr_stream_errors);

  /*
   * Called when new data will be available in the input buffer
   */
  enum olsr_stream_session_state (*receive_data)(struct olsr_stream_session *);
};

/*
 * Represents a TCP server socket or a configuration for a set of outgoing
 * TCP streams.
 */
struct olsr_stream_socket {
  struct list_entity node;

  union netaddr_socket local_socket;

  struct list_entity session;

  struct olsr_socket_entry scheduler_entry;

  struct olsr_stream_config config;

  bool busy;
  bool remove;
  bool remove_when_finished;
};

struct olsr_stream_managed {
  struct olsr_stream_socket socket_v4;
  struct olsr_stream_socket socket_v6;
  struct olsr_netaddr_acl acl;

  struct olsr_stream_config config;
};

struct olsr_stream_managed_config {
  struct olsr_netaddr_acl acl;
  struct netaddr bindto_v4;
  struct netaddr bindto_v6;
  uint16_t port;
};

EXPORT void olsr_stream_init(void);
EXPORT void olsr_stream_cleanup(void);

EXPORT int olsr_stream_add(struct olsr_stream_socket *,
    union netaddr_socket *local);
EXPORT void olsr_stream_remove(struct olsr_stream_socket *, bool force);
EXPORT struct olsr_stream_session *olsr_stream_connect_to(
    struct olsr_stream_socket *, union netaddr_socket *remote);
EXPORT void olsr_stream_flush(struct olsr_stream_session *con);

EXPORT void olsr_stream_set_timeout(
    struct olsr_stream_session *con, uint64_t timeout);
EXPORT void olsr_stream_close(struct olsr_stream_session *con, bool force);

EXPORT void olsr_stream_add_managed(struct olsr_stream_managed *);
EXPORT int olsr_stream_apply_managed(struct olsr_stream_managed *,
    struct olsr_stream_managed_config *);
EXPORT void olsr_stream_remove_managed(struct olsr_stream_managed *, bool force);

#endif /* OLSR_STREAM_SOCKET_H_ */

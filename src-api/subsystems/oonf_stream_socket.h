
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

#ifndef OONF_STREAM_SOCKET_H_
#define OONF_STREAM_SOCKET_H_

#include "common/common_types.h"
#include "common/autobuf.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"

#include "subsystems/oonf_class.h"
#include "subsystems/oonf_socket.h"
#include "subsystems/oonf_timer.h"

enum oonf_stream_session_state {
  STREAM_SESSION_ACTIVE,
  STREAM_SESSION_SEND_AND_QUIT,
  STREAM_SESSION_CLEANUP,
};

enum oonf_stream_errors {
  STREAM_REQUEST_FORBIDDEN = 403,
  STREAM_REQUEST_TOO_LARGE = 413,
  STREAM_SERVICE_UNAVAILABLE = 503,
};

/* represents a TCP stream */
struct oonf_stream_session {
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
  struct oonf_stream_socket *comport;

  /* scheduler handler for the session */
  struct oonf_socket_entry scheduler_entry;

  /* timer for handling session timeout */
  struct oonf_timer_entry timeout;

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

  enum oonf_stream_session_state state;
};

struct oonf_stream_config {
  /* memory cookie to allocate struct for tcp session */
  struct oonf_class *memcookie;

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
  struct netaddr_acl *acl;

  /* Called when a new session is created */
  int (*init)(struct oonf_stream_session *);

  /* Called when a TCP session ends */
  void (*cleanup)(struct oonf_stream_session *);

  /*
   * An error happened during parsing the TCP session,
   * the user of the session might want to create an error message
   */
  void (*create_error)(struct oonf_stream_session *, enum oonf_stream_errors);

  /*
   * Called when new data will be available in the input buffer
   */
  enum oonf_stream_session_state (*receive_data)(struct oonf_stream_session *);
};

/*
 * Represents a TCP server socket or a configuration for a set of outgoing
 * TCP streams.
 */
struct oonf_stream_socket {
  struct list_entity node;

  union netaddr_socket local_socket;

  struct list_entity session;

  struct oonf_socket_entry scheduler_entry;

  struct oonf_stream_config config;

  bool busy;
  bool remove;
  bool remove_when_finished;
};

struct oonf_stream_managed {
  struct oonf_stream_socket socket_v4;
  struct oonf_stream_socket socket_v6;
  struct netaddr_acl acl;

  struct oonf_stream_config config;
};

struct oonf_stream_managed_config {
  struct netaddr_acl acl;
  struct netaddr bindto_v4;
  struct netaddr bindto_v6;
  int32_t port;
};

#define LOG_STREAM oonf_stream_socket_subsystem.logging
EXPORT extern struct oonf_subsystem oonf_stream_socket_subsystem;

EXPORT int oonf_stream_add(struct oonf_stream_socket *,
    const union netaddr_socket *local);
EXPORT void oonf_stream_remove(struct oonf_stream_socket *, bool force);
EXPORT struct oonf_stream_session *oonf_stream_connect_to(
    struct oonf_stream_socket *, const union netaddr_socket *remote);
EXPORT void oonf_stream_flush(struct oonf_stream_session *con);

EXPORT void oonf_stream_set_timeout(
    struct oonf_stream_session *con, uint64_t timeout);
EXPORT void oonf_stream_close(struct oonf_stream_session *con, bool force);

EXPORT void oonf_stream_add_managed(struct oonf_stream_managed *);
EXPORT int oonf_stream_apply_managed(struct oonf_stream_managed *,
    struct oonf_stream_managed_config *);
EXPORT void oonf_stream_remove_managed(struct oonf_stream_managed *, bool force);

#endif /* OONF_STREAM_SOCKET_H_ */


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

#ifndef OLSR_TELNET_H_
#define OLSR_TELNET_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "core/olsr_stream_socket.h"

enum olsr_telnet_result {
  TELNET_RESULT_ACTIVE,
  TELNET_RESULT_CONTINOUS,
  TELNET_RESULT_INTERNAL_ERROR,
  TELNET_RESULT_QUIT,

  /*
   * this one is used internally for the telnet API,
   * it should not be returned by a command handler
   */
  _TELNET_RESULT_UNKNOWN_COMMAND,
};

/*
 * represents a cleanup handler that must be called when the
 * telnet core is shut down.
 */
struct olsr_telnet_cleanup {
  /* pointer to telnet data */
  struct olsr_telnet_data *data;

  /* callback for cleanup */
  void (*cleanup_handler)(struct olsr_telnet_cleanup *);

  /* custom data pointer for cleanup handler */
  void *custom;

  /* node for list of cleanup handlers */
  struct list_entity node;
};

/*
 * represents the data part of a telnet connection to a client
 */
struct olsr_telnet_data {
  /* address of remote communication partner */
  struct netaddr *remote;

  /* output buffer for telnet commands */
  struct autobuf *out;

  /* current telnet command and parameters */
  const char *command;
  const char *parameter;

  /* remember if echo mode is active */
  bool show_echo;

  /* millisecond timeout between commands */
  uint32_t timeout_value;

  /* callback and data to stop a continous output txt command */
  void (*stop_handler)(struct olsr_telnet_data *);
  void *stop_data[4];

  struct list_entity cleanup_list;
};

/*
 * represents a full telnet session including socket
 */
struct olsr_telnet_session {
  struct olsr_stream_session session;
  struct olsr_telnet_data data;
};

typedef enum olsr_telnet_result (*olsr_telnethandler)
    (struct olsr_telnet_data *con);

#if !defined(REMOVE_HELPTEXT)
#define TELNET_CMD(cmd, cb, helptext, args...) { .command = (cmd), .handler = (cb), .help = helptext, ##args }
#else
#define TELNET_CMD(cmd, cb, helptext, args...) { .command = (cmd), .handler = (cb), .help = "", ##args }
#endif

/* represents a telnet command */
struct olsr_telnet_command {
  /* name of telnet command */
  const char *command;

  /* help text for telnet command, NULL if it uses a custom help handler */
  const char *help;

  /* access control list for telnet command, NULL if not used */
  struct netaddr_acl *acl;

  /* handler for telnet command */
  olsr_telnethandler handler;

  /* handler for help text */
  olsr_telnethandler help_handler;

  /* node for tree of telnet commands */
  struct avl_node _node;
};

EXPORT extern struct oonf_subsystem oonf_telnet_subsystem;

EXPORT struct avl_tree telnet_cmd_tree;

EXPORT int olsr_telnet_add(struct olsr_telnet_command *command);
EXPORT void olsr_telnet_remove(struct olsr_telnet_command *command);

EXPORT void olsr_telnet_stop(struct olsr_telnet_data *data);

EXPORT enum olsr_telnet_result olsr_telnet_execute(
    const char *cmd, const char *para,
    struct autobuf *out, struct netaddr *remote);

/**
 * Add a cleanup handler to a telnet session
 * @param data pointer to telnet data
 * @param cleanup pointer to initialized cleanup handler
 */
static INLINE void
olsr_telnet_add_cleanup(struct olsr_telnet_data *data,
    struct olsr_telnet_cleanup *cleanup) {
  cleanup->data = data;
  list_add_tail(&data->cleanup_list, &cleanup->node);
}

/**
 * Removes a cleanup handler to a telnet session
 * @param cleanup pointer to cleanup handler
 */
static INLINE void
olsr_telnet_remove_cleanup(struct olsr_telnet_cleanup *cleanup) {
  list_remove(&cleanup->node);
}

/**
 * Flushs the output stream of a telnet session. This will be only
 * necessary for continous output.
 * @param data pointer to telnet data
 */
static INLINE void
olsr_telnet_flush_session(struct olsr_telnet_data *data) {
  struct olsr_telnet_session *session;

  session = container_of(data, struct olsr_telnet_session, data);
  olsr_stream_flush(&session->session);
}

#endif /* OLSR_TELNET_H_ */

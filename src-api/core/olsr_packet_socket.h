
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

#ifndef OLSR_PACKET_SOCKET_H_
#define OLSR_PACKET_SOCKET_H_

#include "common/common_types.h"
#include "common/list.h"
#include "common/autobuf.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "core/olsr_interface.h"
#include "core/olsr_socket.h"

#ifndef _WIN32
#include <net/if.h>
#else
#define IF_NAMESIZE 16
#endif

struct olsr_packet_socket;

struct olsr_packet_config {
  void *input_buffer;
  size_t input_buffer_length;

  void (*receive_data)(struct olsr_packet_socket *,
      union netaddr_socket *from, size_t length);

  void *user;
};

struct olsr_packet_socket {
  struct list_entity node;

  struct olsr_socket_entry scheduler_entry;
  union netaddr_socket local_socket;
  struct autobuf out;

  struct olsr_interface_data *interface;

  struct olsr_packet_config config;
};

struct olsr_packet_managed_config {
  struct netaddr_acl acl;
  char interface[IF_NAMESIZE];
  struct netaddr bindto_v4, multicast_v4;
  struct netaddr bindto_v6, multicast_v6;
  int port, multicast_port;
  bool loop_multicast;
  bool mesh;
};

struct olsr_packet_managed {
  struct olsr_packet_socket socket_v4, multicast_v4;
  struct olsr_packet_socket socket_v6, multicast_v6;

  struct olsr_packet_config config;

  /* callback fired when someone applies settings to the socket */
  void (*cb_settings_change)(struct olsr_packet_managed *, bool changed);

  struct olsr_packet_managed_config _managed_config;
  struct olsr_interface_listener _if_listener;
};

EXPORT extern struct oonf_subsystem oonf_packet_socket_subsystem;

EXPORT int olsr_packet_add(struct olsr_packet_socket *,
    union netaddr_socket *local, struct olsr_interface_data *);
EXPORT void olsr_packet_remove(struct olsr_packet_socket *, bool);

EXPORT int olsr_packet_send(struct olsr_packet_socket *,
    union netaddr_socket *remote, const void *data, size_t length);
EXPORT int olsr_packet_send_managed(struct olsr_packet_managed *,
    union netaddr_socket *remote, const void *data, size_t length);
EXPORT int olsr_packet_send_managed_multicast(
    struct olsr_packet_managed *managed,
    const void *data, size_t length, int af_type);
EXPORT void olsr_packet_add_managed(struct olsr_packet_managed *);
EXPORT int olsr_packet_apply_managed(struct olsr_packet_managed *,
    struct olsr_packet_managed_config *);
EXPORT void olsr_packet_remove_managed(struct olsr_packet_managed *, bool force);
EXPORT bool olsr_packet_managed_is_active(
    struct olsr_packet_managed *managed, int af_type);

/**
 * @param sock pointer to packet socket
 * @return true if the socket is active to send data, false otherwise
 */
static INLINE bool
olsr_packet_is_active(struct olsr_packet_socket *sock) {
  return list_is_node_added(&sock->node);
}

#endif /* OLSR_PACKET_SOCKET_H_ */

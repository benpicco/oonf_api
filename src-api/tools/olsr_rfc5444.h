
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2021, the olsr.org team - see HISTORY file
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

#ifndef OLSR_RFC5444_H_
#define OLSR_RFC5444_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "common/netaddr.h"
#include "rfc5444/rfc5444_reader.h"
#include "rfc5444/rfc5444_writer.h"
#include "core/olsr_packet_socket.h"
#include "core/olsr_timer.h"

/* suggested priorities for RFC5444 readers */

enum {
  RFC5444_VALIDATOR_PRIORITY = -256,
  RFC5444_MAIN_PARSER_PRIORITY = 0,
  RFC5444_PLUGIN_PARSER_PRIORITY = 256,
};

/* Configuration section for global rfc5444 settings */
#define CFG_RFC5444_SECTION "rfc5444"

/*
 * Configuration settings for interfaces with rfc5444 default protocol
 * (see RFC 5498, IANA Allocations for Mobile Ad Hoc Network (MANET) Protocols
 */
#define CFG_INTERFACE_SECTION "interface"

/* Maximum packet size for this RFC5444 multiplexer */
#define RFC5444_MAX_PACKET_SIZE (1500-20-8)

/* Maximum message size for this RFC5444 multiplexer */
#define RFC5444_MAX_MESSAGE_SIZE (1280-40-8-3)

/* Maximum buffer size for address TLVs before splitting */
#define RFC5444_ADDRTLV_BUFFER (8192)

/* Protocol name for IANA allocated MANET port */
#define RFC5444_PROTOCOL "rfc5444_default"

/* Interface name for unicast targets */
#define RFC5444_UNICAST_TARGET "any"

struct olsr_rfc5444_target;

struct olsr_rfc5444_protocol {
  char name[32];
  uint16_t port;
  bool fixed_local_port;

  union netaddr_socket *input_address;
  struct olsr_rfc5444_interface *input_interface;

  struct rfc5444_reader reader;
  struct rfc5444_writer writer;

  struct avl_node _node;
  struct avl_tree _interface_tree;

  int _refcount;

  uint8_t _msg_buffer[RFC5444_MAX_MESSAGE_SIZE];
  uint8_t _addrtlv_buffer[RFC5444_ADDRTLV_BUFFER];
};

struct olsr_rfc5444_interface {
  char name[IF_NAMESIZE];

  struct olsr_rfc5444_protocol *protocol;

  struct avl_node _node;
  struct avl_tree _target_tree;
  struct list_entity _listener;

  struct olsr_packet_managed _socket;
  struct olsr_packet_managed_config _socket_config;

  struct olsr_rfc5444_target *multicast4, *multicast6;

  int _refcount;
};

struct olsr_rfc5444_interface_listener {
  void (*cb_interface_changed)(struct olsr_rfc5444_interface_listener *, bool);

  struct olsr_rfc5444_interface *interface;
  struct list_entity _node;
};

struct olsr_rfc5444_target {
  struct rfc5444_writer_interface rfc5444_if;

  struct netaddr dst;

  struct olsr_rfc5444_interface *interface;

  uint16_t _seqno;

  struct avl_node _node;
  struct olsr_timer_entry _aggregation;

  int _refcount;
  int _pktseqno_refcount;

  uint8_t _packet_buffer[RFC5444_MAX_PACKET_SIZE];
};

EXPORT int olsr_rfc5444_init(void)  __attribute__((warn_unused_result));
EXPORT void olsr_rfc5444_cleanup(void);

EXPORT struct olsr_rfc5444_protocol *olsr_rfc5444_add_protocol(
    const char *name, bool fixed_local_port);
EXPORT void olsr_rfc5444_remove_protocol(struct olsr_rfc5444_protocol *);
EXPORT void olsr_rfc5444_reconfigure_protocol(
    struct olsr_rfc5444_protocol *, uint16_t port);

EXPORT struct olsr_rfc5444_interface *olsr_rfc5444_add_interface(
    struct olsr_rfc5444_protocol *protocol,
    struct olsr_rfc5444_interface_listener *, const char *name);
EXPORT void olsr_rfc5444_remove_interface(struct olsr_rfc5444_interface *,
    struct olsr_rfc5444_interface_listener *);
EXPORT void olsr_rfc5444_reconfigure_interface(
    struct olsr_rfc5444_interface *interf, struct olsr_packet_managed_config *config);
EXPORT struct olsr_rfc5444_target *olsr_rfc5444_add_target(
    struct olsr_rfc5444_interface *interface, struct netaddr *dst);
EXPORT void olsr_rfc5444_remove_target(struct olsr_rfc5444_target *target);

EXPORT uint16_t olsr_rfc5444_next_target_seqno(struct olsr_rfc5444_target *);

EXPORT enum rfc5444_result olsr_rfc5444_send(
    struct olsr_rfc5444_target *, uint8_t msgid);

static INLINE struct olsr_rfc5444_target *
olsr_rfc5444_get_target_from_message(struct rfc5444_writer_message *msg) {
  assert (msg->if_specific);

  return container_of(msg->specific_if, struct olsr_rfc5444_target, rfc5444_if);
}

static INLINE struct olsr_rfc5444_target *
olsr_rfc5444_get_target_from_provider(struct rfc5444_writer_content_provider *prv) {
  return olsr_rfc5444_get_target_from_message(prv->creator);
}

static INLINE struct olsr_interface *
olsr_rfc5444_get_core_interface(struct olsr_rfc5444_interface *interf) {
  return interf->_socket._if_listener.interface;
}

static INLINE void
olsr_rfc5444_add_target_seqno(struct olsr_rfc5444_target *target) {
  target->_pktseqno_refcount++;
}

static INLINE void
olsr_rfc5444_remove_target_seqno(struct olsr_rfc5444_target *target) {
  if (target->_pktseqno_refcount > 0) {
    target->_pktseqno_refcount--;
  }
}
#endif /* OLSR_RFC5444_H_ */

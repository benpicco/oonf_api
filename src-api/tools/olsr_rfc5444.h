
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

#ifndef OLSR_RFC5444_H_
#define OLSR_RFC5444_H_

#include "common/common_types.h"
#include "common/avl.h"
#include "common/netaddr.h"
#include "rfc5444/rfc5444_reader.h"
#include "rfc5444/rfc5444_writer.h"
#include "core/olsr_packet_socket.h"
#include "core/olsr_timer.h"
#include "tools/olsr_duplicate_set.h"

/* suggested priorities for RFC5444 readers */

enum {
  RFC5444_VALIDATOR_PRIORITY = -256,
  RFC5444_MAIN_PARSER_PRIORITY = 0,
  RFC5444_LQ_PARSER_PRIORITY = 64,
  RFC5444_PLUGIN_PARSER_PRIORITY = 256,
};

/* Configuration section for global mesh settings */
#define CFG_RFC5444_SECTION "mesh"

/*
 * Configuration settings for interfaces with rfc5444 default protocol
 * (see RFC 5498, IANA Allocations for Mobile Ad Hoc Network (MANET) Protocols
 */
#define CFG_INTERFACE_SECTION "interface"

enum {
  /* Maximum packet size for this RFC5444 multiplexer */
  RFC5444_MAX_PACKET_SIZE = 1500-20-8,

  /* Maximum message size for this RFC5444 multiplexer */
  RFC5444_MAX_MESSAGE_SIZE = 1280-40-8-3,

  /* Maximum buffer size for address TLVs before splitting */
  RFC5444_ADDRTLV_BUFFER = 8192,
};

/* Protocol name for IANA allocated MANET port */
#define RFC5444_PROTOCOL "rfc5444_default"

/* Interface name for unicast targets */
#define RFC5444_UNICAST_TARGET "any"

struct olsr_rfc5444_target;

/*
 * Representation of a rfc5444 based protocol
 */
struct olsr_rfc5444_protocol {
  /* name of the protocol */
  char name[32];

  /* port number of the protocol */
  uint16_t port;

  /*
   * true if the local port must be the protocol port,
   * false if it may be random
   */
  bool fixed_local_port;

  /*
   * this variables are only valid during packet processing and contain
   * additional information about the current packet
   */
  struct netaddr *input_address;
  union netaddr_socket *input_socket;
  struct olsr_rfc5444_interface *input_interface;
  bool input_is_multicast;

  /* RFC5444 reader and writer for this protocol instance */
  struct rfc5444_reader reader;
  struct rfc5444_writer writer;

  /* processed set as defined in OLSRv2 */
  struct olsr_duplicate_set processed_set;

  /* forwarded set as defined in OLSRv2 */
  struct olsr_duplicate_set forwarded_set;

  /* node for tree of protocols */
  struct avl_node _node;

  /* tree of interfaces for this protocol */
  struct avl_tree _interface_tree;

  /* reference count of this protocol */
  int _refcount;

  /* number of users who need a packet sequence number for all packets */
  int _pktseqno_refcount;

  /* next protocol message sequence number */
  uint16_t _msg_seqno;

  /* message buffer for protocol */
  uint8_t _msg_buffer[RFC5444_MAX_MESSAGE_SIZE];

  /* buffer for addresstlvs before splitting the message */
  uint8_t _addrtlv_buffer[RFC5444_ADDRTLV_BUFFER];
};

/*
 * Representation of a rfc5444 interface of a protocol
 */
struct olsr_rfc5444_interface {
  /* name of interface */
  char name[IF_NAMESIZE];

  /* backpointer to protocol */
  struct olsr_rfc5444_protocol *protocol;

  /* Node for tree of interfaces in protocol */
  struct avl_node _node;

  /* tree of unicast targets */
  struct avl_tree _target_tree;

  /* tree of interface event listeners of this interface */
  struct list_entity _listener;

  /* socket for this interface */
  struct olsr_packet_managed _socket;

  /* current socket configuration of this interface */
  struct olsr_packet_managed_config _socket_config;

  /* pointer to ipv4/ipv6 targets for this interface */
  struct olsr_rfc5444_target *multicast4, *multicast6;

  /* receive set as defined in OLSRv2 */
  struct olsr_duplicate_set duplicate_set;

  /* number of users of this interface */
  int _refcount;
};

/*
 * Represents a listener to the interface events of a rfc5444 interface
 */
struct olsr_rfc5444_interface_listener {
  /* callback fired when an event happens */
  void (*cb_interface_changed)(struct olsr_rfc5444_interface_listener *, bool changed);

  /* backpointer to interface */
  struct olsr_rfc5444_interface *interface;

  /* node for list of listeners of an interface */
  struct list_entity _node;
};

/*
 * Represents a target (destination IP) of a rfc5444 interface
 */
struct olsr_rfc5444_target {
  /* rfc5444 API representation of the target */
  struct rfc5444_writer_target rfc5444_target;

  /* destination IP */
  struct netaddr dst;

  /* backpointer to interface */
  struct olsr_rfc5444_interface *interface;

  /* node for tree of targets for unicast interfaces */
  struct avl_node _node;

  /* timer for message aggregation on interface */
  struct olsr_timer_entry _aggregation;

  /* number of users of this target */
  int _refcount;

  /* number of users requesting a packet sequence number for this target */
  int _pktseqno_refcount;

  /* last packet sequence number used for this target */
  uint16_t _pktseqno;

  /* packet output buffer for target */
  uint8_t _packet_buffer[RFC5444_MAX_PACKET_SIZE];
};

EXPORT extern struct oonf_subsystem oonf_rfc5444_subsystem;

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

EXPORT enum rfc5444_result olsr_rfc5444_send_if(
    struct olsr_rfc5444_target *, uint8_t msgid);
EXPORT enum rfc5444_result olsr_rfc5444_send_all(
    struct olsr_rfc5444_protocol *protocol,
    uint8_t msgid, rfc5444_writer_targetselector useIf);

/**
 * @param msg pointer to rfc5444 message
 * @return pointer to rfc5444 target used by message
 */
static INLINE struct olsr_rfc5444_target *
olsr_rfc5444_get_target_from_message(struct rfc5444_writer_message *msg) {
  assert (msg->target_specific);

  return container_of(msg->specific_if, struct olsr_rfc5444_target, rfc5444_target);
}

/**
 * @param interf pointer to rfc5444 interface
 * @return pointer to olsr interface
 */
static INLINE struct olsr_interface *
olsr_rfc5444_get_core_interface(struct olsr_rfc5444_interface *interf) {
  return interf->_socket._if_listener.interface;
}

/**
 * @param target pointer to rfc5444 target
 * @return true if the target (address family type) socket is active
 */
static inline bool
olsr_rfc5444_is_target_active(struct olsr_rfc5444_target *target) {
  return olsr_packet_managed_is_active(&target->interface->_socket,
      netaddr_get_address_family(&target->dst));
}

/**
 * Request a protocol wide packet sequence number
 * @param protocol pointer to rfc5444 protocol instance
 */
static INLINE void
olsr_rfc5444_add_protocol_pktseqno(struct olsr_rfc5444_protocol *protocol) {
  protocol->_pktseqno_refcount++;
}

/**
 * Release the request for a protocol wide packet sequence number
 * @param protocol pointer to rfc5444 protocol instance
 */
static INLINE void
olsr_rfc5444_remove_protocol_pktseqno(struct olsr_rfc5444_protocol *protocol) {
  if (protocol->_pktseqno_refcount > 0) {
    protocol->_pktseqno_refcount--;
  }
}

/**
 * Request packet sequence number for a target
 * @param protocol pointer to rfc5444 protocol instance
 */
static INLINE void
olsr_rfc5444_add_target_pktseqno(struct olsr_rfc5444_target *target) {
  target->_pktseqno_refcount++;
}

/**
 * Release the request for a packet sequence number for a target
 * @param protocol pointer to rfc5444 protocol instance
 */
static INLINE void
olsr_rfc5444_remove_target_pktseqno(struct olsr_rfc5444_target *target) {
  if (target->_pktseqno_refcount > 0) {
    target->_pktseqno_refcount--;
  }
}

/**
 * @param target pointer to rfc5444 target instance
 * @return last used packet sequence number on this target
 */
static INLINE uint16_t
olsr_rfc5444_get_last_packet_seqno(struct olsr_rfc5444_target *target) {
  return target->_pktseqno;
}

/**
 * Generates a new message sequence number for a protocol.
 * @param protocol pointer to rfc5444 protocol instance
 * @return new message sequence number
 */
static INLINE uint16_t
olsr_rfc5444_get_next_message_seqno(struct olsr_rfc5444_protocol *protocol) {
  protocol->_msg_seqno++;

  return protocol->_msg_seqno;
}

#endif /* OLSR_RFC5444_H_ */

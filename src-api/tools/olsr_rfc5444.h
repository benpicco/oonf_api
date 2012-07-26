/*
 * olsr_rfc5444.h
 *
 *  Created on: Jun 19, 2012
 *      Author: rogge
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
  void (*cb_interface_changed)(struct olsr_rfc5444_interface_listener *);

  struct olsr_rfc5444_interface *interface;
  struct list_entity _node;
};

struct olsr_rfc5444_target {
  struct rfc5444_writer_interface rfc5444_if;

  struct netaddr dst;

  struct olsr_rfc5444_interface *interface;
  struct avl_node _node;

  struct olsr_timer_entry _aggregation;

  int _refcount;

  uint8_t _packet_buffer[RFC5444_MAX_PACKET_SIZE];
};

EXPORT int olsr_rfc5444_init(void)  __attribute__((warn_unused_result));
EXPORT void olsr_rfc5444_cleanup(void);

EXPORT struct olsr_rfc5444_protocol *olsr_rfc5444_add_protocol(
    const char *name);
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

EXPORT enum rfc5444_result olsr_rfc5444_send(
    struct olsr_rfc5444_target *, uint8_t msgid);

static INLINE struct olsr_rfc5444_target *
olsr_rfc5444_get_target_from_provider(struct rfc5444_writer_content_provider *prv) {
  struct rfc5444_writer_message *msg;

  msg = prv->_creator;
  assert (msg->if_specific);

  return container_of(msg->specific_if, struct olsr_rfc5444_target, rfc5444_if);
}

#endif /* OLSR_RFC5444_H_ */

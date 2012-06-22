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

struct olsr_rfc5444_target {
  struct rfc5444_writer_interface if_ipv4, if_ipv6;

  char name[IF_NAMESIZE];

  struct avl_node _node;

  struct olsr_packet_managed _socket;
  struct olsr_interface_listener _if_listener;

  struct olsr_timer_entry _aggregation_v4, _aggregation_v6;
};

EXPORT void olsr_rfc5444_init(void);
EXPORT void olsr_rfc5444_cleanup(void);

EXPORT struct olsr_rfc5444_target *olsr_rfc5444_get_mc_target(
    const char *name);

EXPORT enum rfc5444_result olsr_rfc5444_send(
    struct olsr_rfc5444_target *, uint8_t msgid, bool ipv4, bool ipv6);

EXPORT struct rfc5444_reader *olsr_rfc5444_get_reader(void);
EXPORT struct rfc5444_writer *olsr_rfc5444_get_writer(void);
EXPORT const union netaddr_socket *olsr_rfc5444_get_source_address(void);

/**
 * @param ptr pointer to RFC5444 target
 * @return olsr interface of target
 */
static INLINE struct olsr_interface *
olsr_rfc5444_get_interface(struct olsr_rfc5444_target *ptr) {
  return ptr->_if_listener.interface;
}

#endif /* OLSR_RFC5444_H_ */

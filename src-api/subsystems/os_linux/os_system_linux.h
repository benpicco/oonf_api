
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

#ifndef OS_SYSTEM_LINUX_H_
#define OS_SYSTEM_LINUX_H_

#include <linux/netlink.h>

#include "common/netaddr.h"
#include "subsystems/oonf_socket.h"
#include "subsystems/oonf_timer.h"

struct os_system_netlink {
  struct oonf_socket_entry socket;
  struct autobuf out;

  struct nlmsghdr *in;
  size_t in_len;

  uint32_t seq_used;
  uint32_t seq_sent;

  int msg_in_transit;

  void (*cb_message)(struct nlmsghdr *hdr);
  void (*cb_error)(uint32_t seq, int error);
  void (*cb_timeout)(void);
  void (*cb_done)(uint32_t seq);

  struct oonf_timer_entry timeout;
};

EXPORT int os_system_netlink_add(struct os_system_netlink *,
    int protocol);
EXPORT void os_system_netlink_remove(struct os_system_netlink *);
EXPORT int os_system_netlink_send(struct os_system_netlink *fd,
    struct nlmsghdr *nl_hdr);
EXPORT int os_system_netlink_add_mc(struct os_system_netlink *,
    const uint32_t *groups, size_t groupcount);
EXPORT int os_system_netlink_drop_mc(struct os_system_netlink *,
    const int *groups, size_t groupcount);

EXPORT int os_system_netlink_addreq(struct nlmsghdr *n,
    int type, const void *data, int len);

static INLINE int
os_system_netlink_addnetaddr(struct nlmsghdr *n,
    int type, const struct netaddr *addr) {
  return os_system_netlink_addreq(n, type, netaddr_get_binptr(addr), netaddr_get_maxprefix(addr)/8);
}
#endif /* OS_SYSTEM_LINUX_H_ */

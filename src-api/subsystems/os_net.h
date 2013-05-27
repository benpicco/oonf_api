
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

#ifndef OS_NET_H_
#define OS_NET_H_

#include <unistd.h>
#include <sys/select.h>

#include "common/avl.h"
#include "common/common_types.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "core/oonf_logging.h"
#include "subsystems/oonf_timer.h"

struct oonf_interface_data {
  /* Interface addresses with mesh-wide scope (at least) */
  struct netaddr *if_v4, *if_v6;

  /* IPv6 Interface address with global scope */
  struct netaddr *linklocal_v6_ptr;

  /* mac address of interface */
  struct netaddr mac;

  /* list of all addresses of the interface */
  struct netaddr *addresses;
  size_t addrcount;

  /* interface name */
  char name[IF_NAMESIZE];

  /* interface index */
  unsigned index;

  /* true if the interface exists and is up */
  bool up;

  /* true if this is a loopback interface */
  bool loopback;
};

struct oonf_interface {
  /* data of interface */
  struct oonf_interface_data data;

  /*
   * usage counter to allow multiple instances to add the same
   * interface
   */
  int usage_counter;

  /*
   * usage counter to keep track of the number of users on
   * this interface who want to send mesh traffic
   */
  int mesh_counter;

  /*
   * used to store internal state of interfaces before
   * configuring them for manet data forwarding.
   * Only used by os_specific code.
   */
  uint32_t _original_state;

  /* hook interfaces into tree */
  struct avl_node _node;

  /* timer for lazy interface change handling */
  struct oonf_timer_entry _change_timer;
};

/* pre-declare inlines */
static INLINE int os_net_bindto_interface(int, struct oonf_interface_data *data);
static INLINE int os_close(int fd);
static INLINE int os_select(
    int num, fd_set *r,fd_set *w,fd_set *e, struct timeval *timeout);
static INLINE const char *on_net_get_loopback_name(void);

/* include os-specific headers */
#if defined(__linux__)
#include "subsystems/os_linux/os_net_linux.h"
#elif defined (BSD)
#include "subsystems/os_bsd/os_net_bsd.h"
#elif defined (_WIN32)
#include "subsystems/os_win32/os_net_win32.h"
#else
#error "Unknown operation system"
#endif

EXPORT extern struct oonf_subsystem oonf_os_net_subsystem;

/* prototypes for all os_net functions */
EXPORT int os_net_getsocket(union netaddr_socket *bindto,
    bool tcp, int recvbuf, struct oonf_interface_data *, enum oonf_log_source log_src);
EXPORT int os_net_configsocket(int sock, union netaddr_socket *bindto,
    int recvbuf, struct oonf_interface_data *, enum oonf_log_source log_src);
EXPORT int os_net_set_nonblocking(int sock);
EXPORT int os_net_join_mcast_recv(int sock, struct netaddr *multicast,
    struct oonf_interface_data *oif, enum oonf_log_source log_src);
EXPORT int os_net_join_mcast_send(int sock, struct netaddr *multicast,
    struct oonf_interface_data *oif, bool loop, enum oonf_log_source log_src);
EXPORT int os_net_update_interface(struct oonf_interface_data *, const char *);
EXPORT int os_recvfrom(int fd, void *buf, size_t length,
    union netaddr_socket *source, struct oonf_interface_data *);
EXPORT int os_sendto(
    int fd, const void *buf, size_t length, union netaddr_socket *dst);

EXPORT int os_net_init_mesh_if(struct oonf_interface *);
EXPORT void os_net_cleanup_mesh_if(struct oonf_interface *);

#endif /* OS_NET_H_ */

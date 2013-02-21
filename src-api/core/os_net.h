
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

#ifndef OS_NET_H_
#define OS_NET_H_

#include <unistd.h>
#include <sys/select.h>

#include "common/common_types.h"
#include "common/netaddr.h"
#include "core/olsr_logging.h"
#include "core/olsr_interface.h"

/* pre-declare inlines */
static INLINE int os_net_bindto_interface(int, struct olsr_interface_data *data);
static INLINE int os_close(int fd);
static INLINE int os_select(
    int num, fd_set *r,fd_set *w,fd_set *e, struct timeval *timeout);

/* include os-specific headers */
#if defined(__linux__)
#include "core/os_linux/os_net_linux.h"
#elif defined (BSD)
#include "core/os_bsd/os_net_bsd.h"
#elif defined (_WIN32)
#include "core/os_win32/os_net_win32.h"
#else
#error "Unknown operation system"
#endif

/* prototypes for all os_net functions */
EXPORT int os_net_init(void) __attribute__((warn_unused_result));
EXPORT void os_net_cleanup(void);
EXPORT int os_net_getsocket(union netaddr_socket *bindto,
    bool tcp, int recvbuf, struct olsr_interface_data *, enum log_source log_src);
EXPORT int os_net_configsocket(int sock, union netaddr_socket *bindto,
    int recvbuf, struct olsr_interface_data *, enum log_source log_src);
EXPORT int os_net_set_nonblocking(int sock);
EXPORT int os_net_join_mcast_recv(int sock, struct netaddr *multicast,
    struct olsr_interface_data *oif, enum log_source log_src);
EXPORT int os_net_join_mcast_send(int sock, struct netaddr *multicast,
    struct olsr_interface_data *oif, bool loop, enum log_source log_src);
EXPORT int os_net_update_interface(struct olsr_interface_data *, const char *);
EXPORT int os_recvfrom(int fd, void *buf, size_t length,
    union netaddr_socket *source, struct olsr_interface_data *);
EXPORT int os_sendto(
    int fd, const void *buf, size_t length, union netaddr_socket *dst);

#endif /* OS_NET_H_ */

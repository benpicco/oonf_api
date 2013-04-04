
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

#ifndef OS_NET_LINUX_H_
#define OS_NET_LINUX_H_

#include <sys/select.h>
#include <unistd.h>
#include <ifaddrs.h>

#include "core/olsr_interface.h"

/* name of the loopback interface */
#define IF_LOOPBACK_NAME "lo"

/**
 * Close a file descriptor
 * @param fd filedescriptor
 */
static INLINE int
os_close(int fd) {
  return close(fd);
}

/**
 * polls a number of sockets for network events. If no even happens or
 * already has happened, function will return after timeout time.
 * see 'man select' for more details
 * @param num
 * @param r
 * @param w
 * @param e
 * @param timeout
 * @return
 */
static INLINE int
os_select(int num, fd_set *r,fd_set *w,fd_set *e, struct timeval *timeout) {
  return select(num, r, w, e, timeout);
}


/**
 * Binds a socket to a certain interface
 * @param sock filedescriptor of socket
 * @param interf name of interface
 * @return -1 if an error happened, 0 otherwise
 */
static INLINE int
os_net_bindto_interface(int sock, struct olsr_interface_data *data) {
  return setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, data->name, strlen(data->name) + 1);
}

#endif /* OS_NET_LINUX_H_ */


/*
 * The olsr.org Optimized Link-State Routing daemon version 2 (olsrd2)
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
#include <sys/types.h>
#include <sys/socket.h>

#include "subsystems/os_net.h"

/* name of the loopback interface */
#define IF_LOOPBACK_NAME "lo"

EXPORT int os_net_linux_get_ioctl_fd(int af_type);

/**
 * Close a file descriptor
 * @param fd filedescriptor
 * @return -1 if an error happened, 0 otherwise
 */
static INLINE int
os_net_close(int fd) {
  return close(fd);
}

/**
 * Listen to a TCP socket
 * @param fd filedescriptor
 * @param n backlog
 * @return -1 if an error happened, 0 otherwise
 */
static INLINE int
os_net_listen(int fd, int n) {
  return listen(fd, n);
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
os_net_select(int num, fd_set *r,fd_set *w,fd_set *e, struct timeval *timeout) {
  return select(num, r, w, e, timeout);
}

/**
 * Connect TCP socket to remote server
 * @param sockfd filedescriptor
 * @param remote remote socket
 * @return -1 if an error happened, 0 otherwise
 */
static INLINE int
os_net_connect(int sockfd, const union netaddr_socket *remote) {
  return connect(sockfd, &remote->std, sizeof(*remote));
}

static INLINE int
os_net_accept(int sockfd, union netaddr_socket *incoming) {
  socklen_t len = sizeof(*incoming);
  return accept(sockfd, &incoming->std, &len);
}

static INLINE int
os_net_get_socket_error(int fd, int *value) {
  socklen_t len = sizeof(*value);
  return getsockopt(fd, SOL_SOCKET, SO_ERROR, value, &len);
}

/**
 * Sends data to an UDP socket.
 * @param fd filedescriptor
 * @param buf buffer for target data
 * @param length length of buffer
 * @param dst pointer to netaddr socket to send packet to
 * @return same as sendto()
 */
static INLINE int
os_net_sendto(int fd, const void *buf, size_t length, const union netaddr_socket *dst) {
  return sendto(fd, buf, length, 0, &dst->std, sizeof(*dst));
}

/**
 * Receive data from a socket.
 * @param fd filedescriptor
 * @param buf buffer for incoming data
 * @param length length of buffer
 * @param source pointer to netaddr socket object to store source of packet
 * @param interf limit received data to certain interface
 *   (only used if socket cannot be bound to interface)
 * @return same as recvfrom()
 */
static INLINE int
os_net_recvfrom(int fd, void *buf, size_t length, union netaddr_socket *source,
    const struct oonf_interface_data *interf __attribute__((unused))) {
  socklen_t len = sizeof(*source);
  return recvfrom(fd, buf, length, 0, &source->std, &len);
}

/**
 * Binds a socket to a certain interface
 * @param sock filedescriptor of socket
 * @param interf name of interface
 * @return -1 if an error happened, 0 otherwise
 */
static INLINE int
os_net_bindto_interface(int sock, struct oonf_interface_data *data) {
  return setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE, data->name, strlen(data->name) + 1);
}

/**
 * @return name of loopback interface
 */
static INLINE const char *
on_net_get_loopback_name(void) {
  return "lo";
}

#endif /* OS_NET_LINUX_H_ */

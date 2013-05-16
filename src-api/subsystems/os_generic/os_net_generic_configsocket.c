
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

#include <errno.h>
#include <fcntl.h>

#include "common/common_types.h"
#include "common/netaddr.h"
#include "common/string.h"
#include "core/oonf_logging.h"
#include "subsystems/oonf_interface.h"
#include "subsystems/os_net.h"

/**
 * Configure a network socket
 * @param sock filedescriptor
 * @param bind_to ip/port to bind the socket to
 * @param recvbuf size of input buffer for socket
 * @param interf pointer to interface to bind socket on,
 *   NULL if socket should not be bound to an interface
 * @param log_src logging source for error messages
 * @return -1 if an error happened, 0 otherwise
 */
int
os_net_configsocket(int sock, union netaddr_socket *bind_to, int recvbuf,
    struct oonf_interface_data *interf __attribute__((unused)),
    enum log_source log_src __attribute__((unused))) {
  int yes;
  socklen_t addrlen;
  union netaddr_socket bindto;
  struct netaddr_str buf;

  /* temporary copy bindto address */
  memcpy(&bindto, bind_to, sizeof(bindto));

  if (os_net_set_nonblocking(sock)) {
    OONF_WARN(log_src, "Cannot make socket non-blocking %s: %s (%d)\n",
        netaddr_socket_to_string(&buf, &bindto), strerror(errno), errno);
    return -1;
  }

#if defined(SO_BINDTODEVICE)
  /* this is no multicast */
  if (interf != NULL && setsockopt(sock, SOL_SOCKET, SO_BINDTODEVICE,
      interf->name, strlen(interf->name) + 1) < 0) {
    OONF_WARN(log_src, "Cannot bind socket to interface %s: %s (%d)\n",
        interf->name, strerror(errno), errno);
    return -1;
  }
#endif

#if defined(SO_REUSEADDR)
  /* allow to reuse address */
  yes = 1;
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
    OONF_WARN(log_src, "Cannot reuse address for %s: %s (%d)\n",
        netaddr_socket_to_string(&buf, &bindto), strerror(errno), errno);
    return -1;
  }
#endif

#if defined(IP_RECVIF)
  if (interf != NULL
      && setsockopt(sock, IPPROTO_IP, IP_RECVIF, &yes, sizeof(yes)) < 0) {
    OONF_WARN(log_src, "Cannot apply IP_RECVIF for %s: %s (%d)\n",
        netaddr_socket_to_string(&buf, &bindto), strerror(errno), errno);
    return -1;
  }
#endif

#if defined(SO_RCVBUF)
  if (recvbuf > 0) {
    while (recvbuf > 8192) {
      if (setsockopt(sock, SOL_SOCKET, SO_RCVBUF,
          (char *)&recvbuf, sizeof(recvbuf)) == 0) {
        break;
      }

      recvbuf -= 1024;
    }

    if (recvbuf < 8192) {
      OONF_WARN(log_src, "Cannot setup receive buffer size for %s: %s (%d)\n",
          netaddr_socket_to_string(&buf, &bindto), strerror(errno), errno);
      return -1;
    }
  }
#endif

  /* add ipv6 interface scope if necessary */
  if (interf != NULL && netaddr_socket_get_addressfamily(&bindto) == AF_INET6) {
    bindto.v6.sin6_scope_id = interf->index;
  }

  /* bind the socket to the port number */
  addrlen = sizeof(bindto);
  if (bind(sock, &bindto.std, addrlen) < 0) {
    OONF_WARN(log_src, "Cannot bind socket to address %s: %s (%d)\n",
        netaddr_socket_to_string(&buf, &bindto), strerror(errno), errno);

    return -1;
  }

  return 0;
}

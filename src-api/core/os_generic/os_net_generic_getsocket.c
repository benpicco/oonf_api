
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2012, the olsr.org team - see HISTORY file
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
#include "core/olsr_logging.h"
#include "core/os_net.h"

/**
 * Creates a new socket and configures it
 * @param bind_to address to bind the socket to
 * @param tcp true for a TCP socket, false for UDP
 * @param recvbuf size of input buffer for socket
 * @param interf pointer to interface to bind socket on,
 *   NULL if socket should not be bound to an interface
 * @param log_src logging source for error messages
 * @return socket filedescriptor, -1 if an error happened
 */
int
os_net_getsocket(union netaddr_socket *bind_to,
    bool tcp, int recvbuf, struct olsr_interface_data *interf,
    enum log_source log_src __attribute__((unused))) {

  int sock;

  sock = socket(bind_to->std.sa_family,
      tcp ? SOCK_STREAM : SOCK_DGRAM, 0);
  if (sock < 0) {
    OLSR_WARN(log_src, "Cannot open socket: %s (%d)", strerror(errno), errno);
    return -1;
  }

  if (os_net_configsocket(sock, bind_to, recvbuf, interf, log_src)) {
    os_close(sock);
    return -1;
  }
  return sock;
}

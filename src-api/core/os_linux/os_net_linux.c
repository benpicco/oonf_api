
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

#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <ifaddrs.h>

#include "common/common_types.h"

#include "core/olsr_cfg.h"
#include "core/olsr_interface.h"
#include "core/olsr_logging.h"
#include "core/olsr.h"
#include "core/os_net.h"

static int _ioctl_v4, _ioctl_v6;

OLSR_SUBSYSTEM_STATE(_os_net_state);

/**
 * Initialize os_net subsystem
 * @return -1 if an error happened, 0 otherwise
 */
int
os_net_init(void) {
  if (olsr_subsystem_is_initialized(&_os_net_state))
    return 0;

  _ioctl_v4 = socket(AF_INET, SOCK_DGRAM, 0);
  if (_ioctl_v4 == -1) {
    OLSR_WARN(LOG_OS_NET, "Cannot open ipv4 ioctl socket: %s (%d)",
        strerror(errno), errno);
    return -1;
  }

  if (config_global.ipv6) {
    _ioctl_v6 = socket(AF_INET6, SOCK_DGRAM, 0);
    if (_ioctl_v6 == -1) {
      OLSR_WARN(LOG_OS_NET, "Cannot open ipv6 ioctl socket: %s (%d)",
          strerror(errno), errno);
      close(_ioctl_v4);
      return -1;
    }
  }
  else {
    _ioctl_v6 = -1;
  }

  olsr_subsystem_init(&_os_net_state);
  return 0;
}

/**
 * Cleanup os_net subsystem
 */
void
os_net_cleanup(void) {
  if (olsr_subsystem_cleanup(&_os_net_state))
    return;

  close (_ioctl_v4);
  if (_ioctl_v6 != -1) {
    close (_ioctl_v6);
  }
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
int
os_recvfrom(int fd, void *buf, size_t length, union netaddr_socket *source,
    struct olsr_interface_data *interf __attribute__((unused))) {
  socklen_t len = sizeof(*source);
  return recvfrom(fd, buf, length, 0, &source->std, &len);
}

/**
 * Updates the data of an interface.
 * @param interf pointer to interface object.
 * @param name name of interface
 * @return -1 if an error happened, 0 otherwise
 */
int
os_net_update_interface(struct olsr_interface_data *interf,
    const char *name) {
  struct ifaddrs *ifaddr, *ifa;
  union netaddr_socket *sock;
  struct netaddr addr;
  struct ifreq ifr;

  memset(interf, 0, sizeof(*interf));
  strscpy(interf->name, name, sizeof(interf->name));

  /* get interface index */
  interf->index = if_nametoindex(name);
  if (interf->index == 0) {
    /* interface is not there at the moment */
    return 0;
  }

  if (getifaddrs(&ifaddr) == -1) {
    OLSR_WARN(LOG_OS_NET, "Cannot get interface addresses: %s (%d)",
        strerror(errno), errno);
    return -1;
  }

  for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
    if (strcmp(ifa->ifa_name, interf->name) != 0) {
      continue;
    }

    sock = (union netaddr_socket *)ifa->ifa_addr;

    if (netaddr_from_socket(&addr, sock)) {
      /* just ignore other interfaces */
      continue;
    }

    if (addr.type == AF_INET) {
      memcpy(&interf->if_v4, &addr, sizeof(interf->if_v4));
    }
    else if (addr.type == AF_INET6) {
      if (IN6_IS_ADDR_LINKLOCAL(addr.addr)) {
        memcpy(&interf->linklocal_v6, &addr, sizeof(interf->linklocal_v6));
      }
      else if (!(IN6_IS_ADDR_LOOPBACK(addr.addr)
          || IN6_IS_ADDR_MULTICAST(addr.addr)
          || IN6_IS_ADDR_UNSPECIFIED(addr.addr)
          || IN6_IS_ADDR_V4COMPAT(addr.addr)
          || IN6_IS_ADDR_V4MAPPED(addr.addr))) {
        memcpy(&interf->if_v6, &addr, sizeof(interf->if_v6));
      }
    }
  }
  freeifaddrs(ifaddr);

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, interf->name, IF_NAMESIZE);

  if (ioctl(_ioctl_v4, SIOCGIFFLAGS, &ifr) < 0) {
    OLSR_WARN(LOG_OS_NET,
        "ioctl SIOCGIFFLAGS (get flags) error on device %s: %s (%d)\n",
        interf->name, strerror(errno), errno);
    return -1;
  }

  if ((ifr.ifr_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP|IFF_RUNNING)) {
    interf->up = true;
  }

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, interf->name, IF_NAMESIZE);

  if (ioctl(_ioctl_v4, SIOCGIFHWADDR, &ifr) < 0) {
    OLSR_WARN(LOG_OS_NET,
        "ioctl SIOCGIFHWADDR (get flags) error on device %s: %s (%d)\n",
        interf->name, strerror(errno), errno);
    return -1;
  }

  netaddr_from_binary(&interf->mac, ifr.ifr_hwaddr.sa_data, 6, AF_MAC48);
  return 0;
}

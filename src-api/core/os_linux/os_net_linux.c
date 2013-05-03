
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

#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <errno.h>

#include "common/common_types.h"

#include "core/olsr_interface.h"
#include "core/olsr_logging.h"
#include "core/olsr_subsystem.h"
#include "core/os_net.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

/* global ioctl sockets for ipv4 and ipv6 */
static int _ioctl_v4, _ioctl_v6;

/* subsystem definition */
struct oonf_subsystem oonf_os_net_subsystem = {
  .init = _init,
  .cleanup = _cleanup,
};

/**
 * Initialize os_net subsystem
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  _ioctl_v4 = socket(AF_INET, SOCK_DGRAM, 0);
  if (_ioctl_v4 == -1) {
    OLSR_WARN(LOG_OS_NET, "Cannot open ipv4 ioctl socket: %s (%d)",
        strerror(errno), errno);
    return -1;
  }

  _ioctl_v6 = socket(AF_INET6, SOCK_DGRAM, 0);
  if (_ioctl_v6 == -1) {
    OLSR_WARN(LOG_OS_NET, "Cannot open ipv6 ioctl socket: %s (%d)",
        strerror(errno), errno);

    /* do not stop here, system might just not support IPv6 */
  }

  return 0;
}

/**
 * Cleanup os_net subsystem
 */
static void
_cleanup(void) {
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
 * The interface data object will be completely overwritten
 * @param ifdata pointer to an interface data object
 * @param name name of interface
 * @return -1 if an error happened, 0 otherwise
 */
int
os_net_update_interface(struct olsr_interface_data *ifdata,
    const char *name) {
  struct ifreq ifr;
  struct ifaddrs *ifaddrs;
  struct ifaddrs *ifa;
  size_t addrcount;
  union netaddr_socket *sock;
  struct netaddr *addr;

  /* cleanup data structure */
  if (ifdata->addresses) {
    free(ifdata->addresses);
  }

  memset(ifdata, 0, sizeof(*ifdata));
  strscpy(ifdata->name, name, sizeof(ifdata->name));

  /* get interface index */
  ifdata->index = if_nametoindex(name);
  if (ifdata->index == 0) {
    /* interface is not there at the moment */
    return 0;
  }

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, ifdata->name, IF_NAMESIZE);

  if (ioctl(_ioctl_v4, SIOCGIFFLAGS, &ifr) < 0) {
    OLSR_WARN(LOG_OS_NET,
        "ioctl SIOCGIFFLAGS (get flags) error on device %s: %s (%d)\n",
        ifdata->name, strerror(errno), errno);
    return -1;
  }

  if ((ifr.ifr_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP|IFF_RUNNING)) {
    ifdata->up = true;
  }

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, ifdata->name, IF_NAMESIZE);

  if (ioctl(_ioctl_v4, SIOCGIFHWADDR, &ifr) < 0) {
    OLSR_WARN(LOG_OS_NET,
        "ioctl SIOCGIFHWADDR (get flags) error on device %s: %s (%d)\n",
        ifdata->name, strerror(errno), errno);
    return -1;
  }

  netaddr_from_binary(&ifdata->mac, ifr.ifr_hwaddr.sa_data, 6, AF_MAC48);

  /* get ip addresses */
  ifaddrs = NULL;
  addrcount = 0;

  if (getifaddrs(&ifaddrs)) {
    OLSR_WARN(LOG_OS_NET,
        "getifaddrs() failed: %s (%d)", strerror(errno), errno);
    return -1;
  }

  for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
    if (strcmp(ifdata->name, ifa->ifa_name) == 0 &&
        (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6)) {
      addrcount++;
    }
  }

  ifdata->addresses = calloc(addrcount, sizeof(struct netaddr));
  if (ifdata->addresses == NULL) {
    OLSR_WARN(LOG_OS_NET,
        "Cannot allocate memory for interface %s with %"PRINTF_SIZE_T_SPECIFIER" prefixes",
        ifdata->name, addrcount);
    freeifaddrs(ifaddrs);
    return -1;
  }

  for (ifa = ifaddrs; ifa != NULL; ifa = ifa->ifa_next) {
    if (strcmp(ifdata->name, ifa->ifa_name) == 0 &&
        (ifa->ifa_addr->sa_family == AF_INET || ifa->ifa_addr->sa_family == AF_INET6)) {
      sock = (union netaddr_socket *)ifa->ifa_addr;
      addr = &ifdata->addresses[ifdata->addrcount];

      if (netaddr_from_socket(&ifdata->addresses[ifdata->addrcount], sock) == 0) {
        ifdata->addrcount++;

        if (netaddr_get_address_family(addr) == AF_INET) {
          if (!(netaddr_is_in_subnet(&NETADDR_IPV4_LOOPBACK, addr)
              || netaddr_is_in_subnet(&NETADDR_IPV4_MULTICAST, addr))) {
            ifdata->if_v4 = addr;
          }
        }
        else if (netaddr_get_address_family(addr) == AF_INET6) {
          if (netaddr_is_in_subnet(&NETADDR_IPV6_LINKLOCAL, addr)) {
            ifdata->linklocal_v6_ptr = addr;
          }
          else if (!(netaddr_cmp(&NETADDR_IPV6_LOOPBACK, addr) == 0
              || netaddr_is_in_subnet(&NETADDR_IPV6_MULTICAST, addr)
              || netaddr_is_in_subnet(&NETADDR_IPV6_IPV4COMPATIBLE, addr)
              || netaddr_is_in_subnet(&NETADDR_IPV6_IPV4MAPPED, addr))) {
            ifdata->if_v6 = addr;
          }
        }
      }
    }
  }

  freeifaddrs(ifaddrs);
  return 0;
}

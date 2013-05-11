
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
#include <sys/utsname.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "common/common_types.h"

#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/os_net.h"

/* ip forwarding */
#define PROC_IPFORWARD_V4 "/proc/sys/net/ipv4/ip_forward"
#define PROC_IPFORWARD_V6 "/proc/sys/net/ipv6/conf/all/forwarding"

/* Redirect proc entry */
#define PROC_IF_REDIRECT "/proc/sys/net/ipv4/conf/%s/send_redirects"
#define PROC_ALL_REDIRECT "/proc/sys/net/ipv4/conf/all/send_redirects"

/* IP spoof proc entry */
#define PROC_IF_SPOOF "/proc/sys/net/ipv4/conf/%s/rp_filter"
#define PROC_ALL_SPOOF "/proc/sys/net/ipv4/conf/all/rp_filter"

/* prototypes */
static int _init(void);
static void _cleanup(void);
static void _activate_if_routing(void);
static void _deactivate_if_routing(void);
static bool _is_at_least_linuxkernel_2_6_31(void);
static int _os_linux_writeToProc(const char *file, char *old, char value);

/* global ioctl sockets for ipv4 and ipv6 */
static int _ioctl_v4, _ioctl_v6;

/* subsystem definition */
struct oonf_subsystem oonf_os_net_subsystem = {
  .init = _init,
  .cleanup = _cleanup,
};

/* global procfile state before initialization */
static char _original_rp_filter;
static char _original_icmp_redirect;
static char _original_ipv4_forward;
static char _original_ipv6_forward;

/* counter of mesh interfaces for ip_forward configuration */
static int _mesh_count = 0;

/**
 * Initialize os_net subsystem
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  _ioctl_v4 = socket(AF_INET, SOCK_DGRAM, 0);
  if (_ioctl_v4 == -1) {
    OONF_WARN(LOG_OS_NET, "Cannot open ipv4 ioctl socket: %s (%d)",
        strerror(errno), errno);
    return -1;
  }

  _ioctl_v6 = socket(AF_INET6, SOCK_DGRAM, 0);
  if (_ioctl_v6 == -1) {
    OONF_WARN(LOG_OS_NET, "Cannot open ipv6 ioctl socket: %s (%d)",
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
    struct oonf_interface_data *interf __attribute__((unused))) {
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
os_net_update_interface(struct oonf_interface_data *ifdata,
    const char *name) {
  struct ifreq ifr;
  struct ifaddrs *ifaddrs;
  struct ifaddrs *ifa;
  size_t addrcount;
  union netaddr_socket *sock;
  struct netaddr *addr;
  struct netaddr_str nbuf;

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
    OONF_WARN(LOG_OS_NET,
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
    OONF_WARN(LOG_OS_NET,
        "ioctl SIOCGIFHWADDR (get flags) error on device %s: %s (%d)\n",
        ifdata->name, strerror(errno), errno);
    return -1;
  }

  netaddr_from_binary(&ifdata->mac, ifr.ifr_hwaddr.sa_data, 6, AF_MAC48);
  OONF_INFO(LOG_OS_NET, "Interface %s has mac address %s",
      ifdata->name, netaddr_to_string(&nbuf, &ifdata->mac));

  /* get ip addresses */
  ifaddrs = NULL;
  addrcount = 0;

  if (getifaddrs(&ifaddrs)) {
    OONF_WARN(LOG_OS_NET,
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
    OONF_WARN(LOG_OS_NET,
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

/**
 * Initialize interface for mesh usage
 * @param interf pointer to interface object
 * @return -1 if an error happened, 0 otherwise
 */
int
os_net_init_mesh_if(struct oonf_interface *interf) {
  char procfile[FILENAME_MAX];
  char old_redirect = 0, old_spoof = 0;

  /* handle global ip_forward setting */
  _mesh_count++;
  if (_mesh_count == 1) {
    _activate_if_routing();
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_REDIRECT, interf->data.name);

  if (_os_linux_writeToProc(procfile, &old_redirect, '0')) {
    OONF_WARN(LOG_OS_SYSTEM, "WARNING! Could not disable ICMP redirects! "
        "You should manually ensure that ICMP redirects are disabled!");
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_SPOOF, interf->data.name);

  if (_os_linux_writeToProc(procfile, &old_spoof, '0')) {
    OONF_WARN(LOG_OS_SYSTEM, "WARNING! Could not disable the IP spoof filter! "
        "You should mannually ensure that IP spoof filtering is disabled!");
  }

  interf->_original_state = (old_redirect << 8) | (old_spoof);
  return 0;
}

/**
 * Cleanup interface after mesh usage
 * @param interf pointer to interface object
 */
void
os_net_cleanup_mesh_if(struct oonf_interface *interf) {
  char restore_redirect, restore_spoof;
  char procfile[FILENAME_MAX];

  restore_redirect = (interf->_original_state >> 8) & 255;
  restore_spoof = (interf->_original_state & 255);

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_REDIRECT, interf->data.name);

  if (_os_linux_writeToProc(procfile, NULL, restore_redirect) != 0) {
    OONF_WARN(LOG_OS_SYSTEM, "Could not restore ICMP redirect flag %s to %c",
        procfile, restore_redirect);
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_SPOOF, interf->data.name);

  if (_os_linux_writeToProc(procfile, NULL, restore_spoof) != 0) {
    OONF_WARN(LOG_OS_SYSTEM, "Could not restore IP spoof flag %s to %c",
        procfile, restore_spoof);
  }

  /* handle global ip_forward setting */
  _mesh_count--;
  if (_mesh_count == 0) {
    _deactivate_if_routing();
  }

  interf->_original_state = 0;
  return;
}

static void
_activate_if_routing(void) {
  if (_os_linux_writeToProc(PROC_IPFORWARD_V4, &_original_ipv4_forward, '1')) {
    OONF_WARN(LOG_OS_SYSTEM, "WARNING! Could not activate ip_forward for ipv4! "
        "You should manually ensure that ip_forward for ipv4 is activated!");
  }
  if (_os_linux_writeToProc(PROC_IPFORWARD_V6, &_original_ipv6_forward, '1')) {
    OONF_WARN(LOG_OS_SYSTEM, "WARNING! Could not activate ip_forward for ipv6! "
        "You should manually ensure that ip_forward for ipv6 is activated!");
  }

  if (_os_linux_writeToProc(PROC_ALL_REDIRECT, &_original_icmp_redirect, '0')) {
    OONF_WARN(LOG_OS_SYSTEM, "WARNING! Could not disable ICMP redirects! "
        "You should manually ensure that ICMP redirects are disabled!");
  }

  /* check kernel version and disable global rp_filter */
  if (_is_at_least_linuxkernel_2_6_31()) {
    if (_os_linux_writeToProc(PROC_ALL_SPOOF, &_original_rp_filter, '0')) {
      OONF_WARN(LOG_OS_SYSTEM, "WARNING! Could not disable global rp_filter "
          "(necessary for kernel 2.6.31 and newer)! You should manually "
          "ensure that rp_filter is disabled!");
    }
  }
}

static void
_deactivate_if_routing(void) {
  if (_os_linux_writeToProc(PROC_ALL_REDIRECT, NULL, _original_icmp_redirect) != 0) {
    OONF_WARN(LOG_OS_SYSTEM,
        "WARNING! Could not restore ICMP redirect flag %s to %c!",
        PROC_ALL_REDIRECT, _original_icmp_redirect);
  }

  if (_os_linux_writeToProc(PROC_ALL_SPOOF, NULL, _original_rp_filter)) {
    OONF_WARN(LOG_OS_SYSTEM,
        "WARNING! Could not restore global rp_filter flag %s to %c!",
        PROC_ALL_SPOOF, _original_rp_filter);
  }

  if (_os_linux_writeToProc(PROC_IPFORWARD_V4, NULL, _original_ipv4_forward)) {
    OONF_WARN(LOG_OS_SYSTEM, "WARNING! Could not restore %s to %c!",
        PROC_IPFORWARD_V4, _original_ipv4_forward);
  }
  if (_os_linux_writeToProc(PROC_IPFORWARD_V6, NULL, _original_ipv6_forward)) {
    OONF_WARN(LOG_OS_SYSTEM, "WARNING! Could not restore %s to %c",
        PROC_IPFORWARD_V6, _original_ipv6_forward);
  }
}


/**
 * Overwrite a numeric entry in the procfile system and keep the old
 * value.
 * @param file pointer to filename (including full path)
 * @param old pointer to memory to store old value
 * @param value new value
 * @return -1 if an error happened, 0 otherwise
 */
static int
_os_linux_writeToProc(const char *file, char *old, char value) {
  int fd;
  char rv;

  if (value == 0) {
    /* ignore */
    return 0;
  }

  if ((fd = open(file, O_RDWR)) < 0) {
    OONF_WARN(LOG_OS_SYSTEM,
      "Error, cannot open proc entry %s: %s (%d)\n",
      file, strerror(errno), errno);
    return -1;
  }

  if (read(fd, &rv, 1) != 1) {
    OONF_WARN(LOG_OS_SYSTEM,
      "Error, cannot read proc entry %s: %s (%d)\n",
      file, strerror(errno), errno);
    return -1;
  }

  if (rv != value) {
    if (lseek(fd, SEEK_SET, 0) == -1) {
      OONF_WARN(LOG_OS_SYSTEM,
        "Error, cannot rewind to start on proc entry %s: %s (%d)\n",
        file, strerror(errno), errno);
      return -1;
    }

    if (write(fd, &value, 1) != 1) {
      OONF_WARN(LOG_OS_SYSTEM,
        "Error, cannot write '%c' to proc entry %s: %s (%d)\n",
        value, file, strerror(errno), errno);
    }

    OONF_DEBUG(LOG_OS_SYSTEM, "Writing '%c' (was %c) to %s", value, rv, file);
  }

  close(fd);

  if (old && rv != value) {
    *old = rv;
  }

  return 0;
}

/**
 * @return true if linux kernel is at least 2.6.31
 */
static bool
_is_at_least_linuxkernel_2_6_31(void) {
  struct utsname uts;
  char *next;
  int first = 0, second = 0, third = 0;

  memset(&uts, 0, sizeof(uts));
  if (uname(&uts)) {
    OONF_WARN(LOG_OS_SYSTEM,
        "Error, could not read kernel version: %s (%d)\n",
        strerror(errno), errno);
    return false;
  }

  first = strtol(uts.release, &next, 10);
  /* check for linux 3.x */
  if (first >= 3) {
    return true;
  }

  if (*next != '.') {
    goto kernel_parse_error;
  }

  second = strtol(next+1, &next, 10);
  if (*next != '.') {
    goto kernel_parse_error;
  }

  third = strtol(next+1, NULL, 10);

  /* better or equal than linux 2.6.31 ? */
  return first == 2 && second == 6 && third >= 31;

kernel_parse_error:
  OONF_WARN(LOG_OS_SYSTEM,
      "Error, cannot parse kernel version: %s\n", uts.release);
  return false;
}

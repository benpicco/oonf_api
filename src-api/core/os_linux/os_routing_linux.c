
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

/* must be first because of a problem with linux/rtnetlink.h */
#include <sys/socket.h>

/* and now the rest of the includes */
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <sys/utsname.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "common/common_types.h"
#include "core/olsr_timer.h"
#include "core/olsr_subsystem.h"
#include "core/os_routing.h"
#include "core/os_system.h"

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

static int _routing_set(struct nlmsghdr *msg, struct os_route *route,
    unsigned char rt_type, unsigned char rt_scope);

static bool _is_at_least_linuxkernel_2_6_31(void);
static int _os_linux_writeToProc(const char *file, char *old, char value);

static void _routing_finished(struct os_route *route, int error);
static void _cb_rtnetlink_message(struct nlmsghdr *);
static void _cb_rtnetlink_error(uint32_t seq, int error);
static void _cb_rtnetlink_done(uint32_t seq);
static void _cb_rtnetlink_timeout(void);

/* global procfile state before initialization */
static char _original_rp_filter;
static char _original_icmp_redirect;
static char _original_ipv4_forward;
static char _original_ipv6_forward;

/* counter of mesh interfaces for ip_forward configuration */
static int _mesh_count = 0;

/* netlink socket for route set/get commands */
struct os_system_netlink _rtnetlink_socket = {
  .cb_message = _cb_rtnetlink_message,
  .cb_error = _cb_rtnetlink_error,
  .cb_done = _cb_rtnetlink_done,
  .cb_timeout = _cb_rtnetlink_timeout,
};
struct list_entity _rtnetlink_feedback;

/* subsystem definition */
struct oonf_subsystem oonf_os_routing_subsystem = {
  .init = _init,
  .cleanup = _cleanup,
};

/* default wildcard route */
const struct os_route OS_ROUTE_WILDCARD = {
  .family = AF_UNSPEC,
  .src = { ._type = AF_UNSPEC },
  .gw = { ._type = AF_UNSPEC },
  .dst = { ._type = AF_UNSPEC },
  .table = RT_TABLE_UNSPEC,
  .metric = -1,
  .protocol = RTPROT_UNSPEC,
  .if_index = 0
};

/**
 * Initialize routing subsystem
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  if (os_system_netlink_add(&_rtnetlink_socket, NETLINK_ROUTE)) {
    return -1;
  }

  if (_os_linux_writeToProc(PROC_ALL_REDIRECT, &_original_icmp_redirect, '0')) {
    OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not disable ICMP redirects! "
        "You should manually ensure that ICMP redirects are disabled!");
  }

  /* check kernel version and disable global rp_filter */
  if (_is_at_least_linuxkernel_2_6_31()) {
    if (_os_linux_writeToProc(PROC_ALL_SPOOF, &_original_rp_filter, '0')) {
      OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not disable global rp_filter "
          "(necessary for kernel 2.6.31 and newer)! You should manually "
          "ensure that rp_filter is disabled!");
    }
  }

  list_init_head(&_rtnetlink_feedback);
  return 0;
}

/**
 * Cleanup all resources allocated by the routing subsystem
 */
static void
_cleanup(void) {
  struct os_route *rt, *rt_it;

  list_for_each_element_safe(&_rtnetlink_feedback, rt, _internal._node, rt_it) {
    _routing_finished(rt, 1);
  }

  if (_original_icmp_redirect != 0
      && _os_linux_writeToProc(PROC_ALL_REDIRECT, &_original_icmp_redirect, '0') != 0) {
    OLSR_WARN(LOG_OS_SYSTEM,
        "WARNING! Could not restore ICMP redirect flag %s to %c!",
        PROC_ALL_REDIRECT, _original_icmp_redirect);
  }

  /* check kernel version and disable global rp_filter */
  if (_os_linux_writeToProc(PROC_ALL_SPOOF, &_original_rp_filter, '0')) {
    OLSR_WARN(LOG_OS_SYSTEM,
        "WARNING! Could not restore global rp_filter flag %s to %c!",
        PROC_ALL_SPOOF, _original_rp_filter);
  }

  if (_mesh_count > 0) {
    if (_os_linux_writeToProc(PROC_IPFORWARD_V4, NULL, _original_ipv4_forward)) {
      OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not restore %s to %c!",
          PROC_IPFORWARD_V4, _original_ipv4_forward);
    }
    if (_os_linux_writeToProc(PROC_IPFORWARD_V6, NULL, _original_ipv6_forward)) {
      OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not restore flag %s to %c",
          PROC_IPFORWARD_V6, _original_ipv6_forward);
    }
  }

  os_system_netlink_remove(&_rtnetlink_socket);
}

/**
 * Initialize interface for mesh usage
 * @param interf pointer to interface object
 * @return -1 if an error happened, 0 otherwise
 */
int
os_routing_init_mesh_if(struct olsr_interface *interf) {
  char procfile[FILENAME_MAX];
  char old_redirect = 0, old_spoof = 0;

  if (!oonf_subsystem_is_initialized(&oonf_os_routing_subsystem)) {
    /* make interface listener work without routing core */
    return 0;
  }

  /* handle global ip_forward setting */
  _mesh_count++;
  if (_mesh_count == 1) {
    if (_os_linux_writeToProc(PROC_IPFORWARD_V4, &_original_ipv4_forward, '1')) {
      OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not activate ip_forward for ipv4! "
          "You should manually ensure that ip_forward for ipv4 is activated!");
    }
    if (_os_linux_writeToProc(PROC_IPFORWARD_V6, &_original_ipv6_forward, '1')) {
      OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not activate ip_forward for ipv6! "
          "You should manually ensure that ip_forward for ipv6 is activated!");
    }
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_REDIRECT, interf->data.name);

  if (_os_linux_writeToProc(procfile, &old_redirect, '0')) {
    OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not disable ICMP redirects! "
        "You should manually ensure that ICMP redirects are disabled!");
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_SPOOF, interf->data.name);

  if (_os_linux_writeToProc(procfile, &old_spoof, '0')) {
    OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not disable the IP spoof filter! "
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
os_routing_cleanup_mesh_if(struct olsr_interface *interf) {
  char restore_redirect, restore_spoof;
  char procfile[FILENAME_MAX];

  if (!oonf_subsystem_is_initialized(&oonf_os_routing_subsystem)) {
    /* make interface listener work without routing core */
    return;
  }

  restore_redirect = (interf->_original_state >> 8) & 255;
  restore_spoof = (interf->_original_state & 255);

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_REDIRECT, interf->data.name);

  if (restore_redirect != 0
      && _os_linux_writeToProc(procfile, NULL, restore_redirect) != 0) {
    OLSR_WARN(LOG_OS_SYSTEM, "Could not restore ICMP redirect flag %s to %c",
        procfile, restore_redirect);
  }

  /* Generate the procfile name */
  snprintf(procfile, sizeof(procfile), PROC_IF_SPOOF, interf->data.name);

  if (restore_spoof != 0
      && _os_linux_writeToProc(procfile, NULL, restore_spoof) != 0) {
    OLSR_WARN(LOG_OS_SYSTEM, "Could not restore IP spoof flag %s to %c",
        procfile, restore_spoof);
  }

  /* handle global ip_forward setting */
  _mesh_count--;
  if (_mesh_count == 0) {
    if (_os_linux_writeToProc(PROC_IPFORWARD_V4, NULL, _original_ipv4_forward)) {
      OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not restore %s to %c!",
          PROC_IPFORWARD_V4, _original_ipv4_forward);
    }
    if (_os_linux_writeToProc(PROC_IPFORWARD_V6, NULL, _original_ipv6_forward)) {
      OLSR_WARN(LOG_OS_SYSTEM, "WARNING! Could not restore flag %s to %c",
          PROC_IPFORWARD_V6, _original_ipv6_forward);
    }
  }

  interf->_original_state = 0;
  return;
}

/**
 * Update an entry of the kernel routing table. This call will only trigger
 * the change, the real change will be done as soon as the netlink socket is
 * writable.
 * @param route data of route to be set/removed
 * @param set true if route should be set, false if it should be removed
 * @param del_similar true if similar routes that block this one should be
 *   removed.
 * @return -1 if an error happened, 0 otherwise
 */
int
os_routing_set(struct os_route *route, bool set, bool del_similar) {
  uint8_t buffer[UIO_MAXIOV];
  struct nlmsghdr *msg;
  unsigned char scope;
  struct os_route os_rt;
  int seq;

  memset(buffer, 0, sizeof(buffer));

  /* copy route settings */
  memcpy(&os_rt, route, sizeof(os_rt));

  /* get pointers for netlink message */
  msg = (void *)&buffer[0];

  msg->nlmsg_flags = NLM_F_REQUEST;

  /* set length of netlink message with rtmsg payload */
  msg->nlmsg_len = NLMSG_LENGTH(sizeof(struct rtmsg));

  /* normally all routing operations are UNIVERSE scope */
  scope = RT_SCOPE_UNIVERSE;

  if (set) {
    msg->nlmsg_flags |= NLM_F_CREATE | NLM_F_REPLACE;
    msg->nlmsg_type = RTM_NEWROUTE;
  } else {
    msg->nlmsg_type = RTM_DELROUTE;

    os_rt.protocol = 0;
    netaddr_invalidate(&os_rt.src);

    if (del_similar) {
      /* no interface necessary */
      os_rt.if_index = 0;

      /* as wildcard for fuzzy deletion */
      scope = RT_SCOPE_NOWHERE;
    }
  }

  if (netaddr_get_address_family(&os_rt.gw) == AF_UNSPEC
      && netaddr_get_prefix_length(&os_rt.dst) == netaddr_get_maxprefix(&os_rt.dst)) {
    /* use destination as gateway, to 'force' linux kernel to do proper source address selection */
    os_rt.gw = os_rt.dst;
  }

  if (_routing_set(msg, &os_rt, RTN_UNICAST, scope)) {
    return -1;
  }

  /* cannot fail */
  seq = os_system_netlink_send(&_rtnetlink_socket, msg);

  if (route->cb_finished) {
    list_add_tail(&_rtnetlink_feedback, &route->_internal._node);
    route->_internal.nl_seq = seq;
  }
  return 0;
}

/**
 * Request all routing dataof a certain address family
 * @param route pointer to routing filter
 * @return -1 if an error happened, 0 otherwise
 */
int
os_routing_query(struct os_route *route) {
  uint8_t buffer[UIO_MAXIOV];
  struct nlmsghdr *msg;
  struct rtgenmsg *rt_gen;
  int seq;

  assert (route->cb_finished != NULL && route->cb_get != NULL);
  memset(buffer, 0, sizeof(buffer));

  /* get pointers for netlink message */
  msg = (void *)&buffer[0];
  rt_gen = NLMSG_DATA(msg);

  msg->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

  /* set length of netlink message with rtmsg payload */
  msg->nlmsg_len = NLMSG_LENGTH(sizeof(*rt_gen));

  msg->nlmsg_type = RTM_GETROUTE;
  rt_gen->rtgen_family = route->family;

  seq = os_system_netlink_send(&_rtnetlink_socket, msg);
  if (seq < 0) {
    return -1;
  }

  list_add_tail(&_rtnetlink_feedback, &route->_internal._node);
  route->_internal.nl_seq = seq;
  return 0;
}

/**
 * Stop processing of a routing command
 * @param route pointer to os_route
 */
void
os_routing_interrupt(struct os_route *route) {
  _routing_finished(route, -1);
}

/**
 * Stop processing of a routing command and set error code
 * for callback
 * @param route pointer to os_route
 * @param error error code, 0 if no error
 */
static void
_routing_finished(struct os_route *route, int error) {
  if (list_is_node_added(&route->_internal._node)) {
    /* remove first to prevent any kind of recursive cleanup */
    list_remove(&route->_internal._node);

    if (route->cb_finished) {
      route->cb_finished(route, error);
    }
  }
}

/**
 * Initiatize the an netlink routing message
 * @param msg pointer to netlink message header
 * @param route data to be added to the netlink message
 * @param scope scope of route to be set/removed
 * @return -1 if an error happened, 0 otherwise
 */
static int
_routing_set(struct nlmsghdr *msg, struct os_route *route,
    unsigned char rt_type, unsigned char rt_scope) {
  struct rtmsg *rt_msg;

  /* calculate address af_type */
  if (netaddr_get_address_family(&route->dst) != AF_UNSPEC) {
    route->family = netaddr_get_address_family(&route->dst);
  }
  if (netaddr_get_address_family(&route->gw) != AF_UNSPEC) {
    if (route->family  != AF_UNSPEC
        && route->family  != netaddr_get_address_family(&route->gw)) {
      return -1;
    }
    route->family  = netaddr_get_address_family(&route->gw);
  }
  if (netaddr_get_address_family(&route->src) != AF_UNSPEC) {
    if (route->family  != AF_UNSPEC && route->family  != netaddr_get_address_family(&route->src)) {
      return -1;
    }
    route->family  = netaddr_get_address_family(&route->src);
  }

  if (route->family  == AF_UNSPEC) {
    route->family  = AF_INET;
  }

  /* initialize rtmsg payload */
  rt_msg = NLMSG_DATA(msg);

  rt_msg->rtm_family = route->family ;
  rt_msg->rtm_scope = rt_scope;
  rt_msg->rtm_type = rt_type;
  rt_msg->rtm_protocol = route->protocol;
  rt_msg->rtm_table = route->table;

  /* add attributes */
  if (netaddr_get_address_family(&route->src) != AF_UNSPEC) {
    rt_msg->rtm_src_len = netaddr_get_prefix_length(&route->src);

    /* add src-ip */
    if (os_system_netlink_addnetaddr(msg, RTA_PREFSRC, &route->src)) {
      return -1;
    }
  }

  if (netaddr_get_address_family(&route->gw) != AF_UNSPEC) {
    rt_msg->rtm_flags |= RTNH_F_ONLINK;

    /* add gateway */
    if (os_system_netlink_addnetaddr(msg, RTA_GATEWAY, &route->gw)) {
      return -1;
    }
  }

  if (netaddr_get_address_family(&route->dst) != AF_UNSPEC) {
    rt_msg->rtm_dst_len = netaddr_get_prefix_length(&route->dst);

    /* add destination */
    if (os_system_netlink_addnetaddr(msg, RTA_DST, &route->dst)) {
      return -1;
    }
  }

  if (route->metric != -1) {
    /* add metric */
    if (os_system_netlink_addreq(msg, RTA_PRIORITY, &route->metric, sizeof(route->metric))) {
      return -1;
    }
  }

  if (route->if_index) {
    /* add interface*/
    if (os_system_netlink_addreq(msg, RTA_OIF, &route->if_index, sizeof(route->if_index))) {
      return -1;
    }
  }
  return 0;
}

/**
 * Parse a rtnetlink header into a os_route object
 * @param route pointer to target os_route
 * @param msg pointer to rtnetlink message header
 * @return -1 if address family of rtnetlink is unknown,
 *   0 otherwise
 */
static int
_routing_parse_nlmsg(struct os_route *route, struct nlmsghdr *msg) {
  struct rtmsg *rt_msg;
  struct rtattr *rt_attr;
  int rt_len;

  rt_msg = NLMSG_DATA(msg);
  rt_attr = (struct rtattr *) RTM_RTA(rt_msg);
  rt_len = RTM_PAYLOAD(msg);

  memcpy(route, &OS_ROUTE_WILDCARD, sizeof(*route));

  route->protocol = rt_msg->rtm_protocol;
  route->table = rt_msg->rtm_table;
  route->family = rt_msg->rtm_family;

  if (route->family != AF_INET && route->family != AF_INET6) {
    return -1;
  }

  for(; RTA_OK(rt_attr, rt_len); rt_attr = RTA_NEXT(rt_attr,rt_len)) {
    switch(rt_attr->rta_type) {
      case RTA_SRC:
        netaddr_from_binary_prefix(&route->src, RTA_DATA(rt_attr), RTA_PAYLOAD(rt_attr),
            rt_msg->rtm_family, rt_msg->rtm_src_len);
        break;
      case RTA_GATEWAY:
        netaddr_from_binary(&route->gw, RTA_DATA(rt_attr), RTA_PAYLOAD(rt_attr), rt_msg->rtm_family);
        break;
      case RTA_DST:
        netaddr_from_binary_prefix(&route->dst, RTA_DATA(rt_attr), RTA_PAYLOAD(rt_attr),
            rt_msg->rtm_family, rt_msg->rtm_dst_len);
        break;
      case RTA_PRIORITY:
        memcpy(&route->metric, RTA_DATA(rt_attr), sizeof(route->metric));
        break;
      case RTA_OIF:
        memcpy(&route->if_index, RTA_DATA(rt_attr), sizeof(route->if_index));
        break;
      default:
        break;
    }
  }

  if (netaddr_get_address_family(&route->dst) == AF_UNSPEC) {
    memcpy(&route->dst, route->family == AF_INET ? &NETADDR_IPV4_ANY : &NETADDR_IPV6_ANY,
        sizeof(route->dst));
    netaddr_set_prefix_length(&route->dst, rt_msg->rtm_dst_len);
  }
  return 0;
}

/**
 * Checks if a os_route object matches a routing filter
 * @param filter pointer to filter
 * @param route pointer to route object
 * @return true if route matches the filter, false otherwise
 */
static bool
_match_routes(struct os_route *filter, struct os_route *route) {
  if (filter->family != route->family) {
    return false;
  }
  if (netaddr_get_address_family(&filter->src) != AF_UNSPEC
      && memcmp(&filter->src, &route->src, sizeof(filter->src)) != 0) {
    return false;
  }
  if (netaddr_get_address_family(&filter->gw) != AF_UNSPEC
      && memcmp(&filter->gw, &route->gw, sizeof(filter->gw)) != 0) {
    return false;
  }
  if (netaddr_get_address_family(&filter->dst) != AF_UNSPEC
      && memcmp(&filter->dst, &route->dst, sizeof(filter->dst)) != 0) {
    return false;
  }
  if (filter->metric != -1 && filter->metric != route->metric) {
    return false;
  }
  if (filter->table != RT_TABLE_UNSPEC && filter->table != route->table) {
    return false;
  }
  if (filter->protocol != RTPROT_UNSPEC && filter->protocol != route->protocol) {
    return false;
  }
  return filter->if_index == 0 || filter->if_index == route->if_index;
}

/**
 * Handle incoming rtnetlink messages
 * @param msg
 */
static void
_cb_rtnetlink_message(struct nlmsghdr *msg) {
  struct os_route *filter;
  struct os_route rt;

  OLSR_DEBUG(LOG_OS_ROUTING, "Got message: %d %d", msg->nlmsg_seq, msg->nlmsg_type);

  if (msg->nlmsg_type != RTM_NEWROUTE && msg->nlmsg_type != RTM_DELROUTE) {
    return;
  }

  if (_routing_parse_nlmsg(&rt, msg)) {
    OLSR_WARN(LOG_OS_ROUTING, "Error while processing route reply");
    return;
  }

  list_for_each_element(&_rtnetlink_feedback, filter, _internal._node) {
    OLSR_DEBUG_NH(LOG_OS_ROUTING, "  Compare with seq: %d", filter->_internal.nl_seq);
    if (msg->nlmsg_seq == filter->_internal.nl_seq) {
      if (filter->cb_get != NULL && _match_routes(filter, &rt)) {
        filter->cb_get(filter, &rt);
      }
      break;
    }
  }
}

/**
 * Handle feedback from netlink socket
 * @param seq
 * @param error
 */
static void
_cb_rtnetlink_error(uint32_t seq, int error) {
  struct os_route *route;

  OLSR_DEBUG(LOG_OS_ROUTING, "Got feedback: %d %d", seq, error);

  /* transform into errno number */
  error = -error;

  list_for_each_element(&_rtnetlink_feedback, route, _internal._node) {
    if (seq == route->_internal.nl_seq) {
      _routing_finished(route, error);
      break;
    }
  }
}

/**
 * Handle ack timeout from netlink socket
 */
static void
_cb_rtnetlink_timeout(void) {
  struct os_route *route, *rt_it;

  OLSR_DEBUG(LOG_OS_ROUTING, "Got timeout");

  list_for_each_element_safe(&_rtnetlink_feedback, route, _internal._node, rt_it) {
    _routing_finished(route, -1);
  }
}

/**
 * Handle done from multipart netlink messages
 * @param seq
 */
static void
_cb_rtnetlink_done(uint32_t seq) {
  struct os_route *route;

  OLSR_DEBUG(LOG_OS_ROUTING, "Got done: %u", seq);

  list_for_each_element(&_rtnetlink_feedback, route, _internal._node) {
    if (seq == route->_internal.nl_seq) {
      _routing_finished(route, 0);
      break;
    }
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

  if ((fd = open(file, O_RDWR)) < 0) {
    goto writetoproc_error;
  }

  if (read(fd, &rv, 1) != 1) {
    goto writetoproc_error;
  }

  if (rv != value) {
    if (lseek(fd, SEEK_SET, 0) == -1) {
      goto writetoproc_error;
    }

    if (write(fd, &value, 1) != 1) {
      goto writetoproc_error;
    }

    OLSR_DEBUG(LOG_OS_SYSTEM, "Writing '%c' (was %c) to %s", value, rv, file);
  }

  if (close(fd) != 0) {
    goto writetoproc_error;
  }

  if (old && rv != value) {
    *old = rv;
  }

  return 0;

writetoproc_error:
  OLSR_WARN(LOG_OS_SYSTEM,
    "Error, cannot read proc entry %s: %s (%d)\n",
    file, strerror(errno), errno);
  return -1;
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
    OLSR_WARN(LOG_OS_SYSTEM,
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
  OLSR_WARN(LOG_OS_SYSTEM,
      "Error, cannot parse kernel version: %s\n", uts.release);
  return false;
}

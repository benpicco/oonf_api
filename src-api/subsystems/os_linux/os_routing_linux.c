
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

#include "common/common_types.h"
#include "core/oonf_subsystem.h"
#include "subsystems/os_routing.h"
#include "subsystems/os_system.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static int _routing_set(struct nlmsghdr *msg, struct os_route *route,
    unsigned char rt_type, unsigned char rt_scope);

static void _routing_finished(struct os_route *route, int error);
static void _cb_rtnetlink_message(struct nlmsghdr *);
static void _cb_rtnetlink_error(uint32_t seq, int error);
static void _cb_rtnetlink_done(uint32_t seq);
static void _cb_rtnetlink_timeout(void);

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
  .name = "os_routing",
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

  os_system_netlink_remove(&_rtnetlink_socket);
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

  OONF_DEBUG(LOG_OS_ROUTING, "Got message: %d %d", msg->nlmsg_seq, msg->nlmsg_type);

  if (msg->nlmsg_type != RTM_NEWROUTE && msg->nlmsg_type != RTM_DELROUTE) {
    return;
  }

  if (_routing_parse_nlmsg(&rt, msg)) {
    OONF_WARN(LOG_OS_ROUTING, "Error while processing route reply");
    return;
  }

  list_for_each_element(&_rtnetlink_feedback, filter, _internal._node) {
    OONF_DEBUG_NH(LOG_OS_ROUTING, "  Compare with seq: %d", filter->_internal.nl_seq);
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

  OONF_DEBUG(LOG_OS_ROUTING, "Got feedback: %d %d", seq, error);

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

  OONF_DEBUG(LOG_OS_ROUTING, "Got timeout");

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

  OONF_DEBUG(LOG_OS_ROUTING, "Got done: %u", seq);

  list_for_each_element(&_rtnetlink_feedback, route, _internal._node) {
    if (seq == route->_internal.nl_seq) {
      _routing_finished(route, 0);
      break;
    }
  }
}


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

#ifndef OS_ROUTING_H_
#define OS_ROUTING_H_

#include <stdio.h>
#include <sys/time.h>

#include "common/common_types.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "subsystems/olsr_interface.h"
#include "core/olsr_logging.h"
#include "subsystems/os_system.h"

/* include os-specific headers */
#if defined(__linux__)
#include "subsystems/os_linux/os_routing_linux.h"
#elif defined (BSD)
#include "subsystems/os_bsd/os_routing_bsd.h"
#elif defined (_WIN32)
#include "subsystems/os_win32/os_routing_win32.h"
#else
#error "Unknown operation system"
#endif

/* make sure default values for routing are there */
#ifndef RTPROT_UNSPEC
#define RTPROT_UNSPEC 0
#endif
#ifndef RT_TABLE_UNSPEC
#define RT_TABLE_UNSPEC 0
#endif

struct os_route_str {
  char buf[
           /* header */
           1+
           /* src */
           5 + sizeof(struct netaddr_str)
           /* gw */
           + 4 + sizeof(struct netaddr_str)
           /* dst */
           + 5 + sizeof(struct netaddr_str)
           /* metric */
           + 7 +11
           /* table, protocol */
           +6+4 +9+4
           +3 + IF_NAMESIZE + 2 + 10 + 2
           /* footer and 0-byte */
           + 2];
};

struct os_route {
  /* used for delivering feedback about netlink commands */
  struct os_route_internal _internal;

  /* address family */
  unsigned char family;

  /* source, gateway and destination */
  struct netaddr src, gw, dst;

  /* metric of the route */
  int metric;

  /* routing table and routing protocol */
  unsigned char table, protocol;

  /* index of outgoing interface */
  unsigned int if_index;

  /* callback when operation is finished */
  void (*cb_finished)(struct os_route *, int error);

  /* callback for os_routing_query() */
  void (*cb_get)(struct os_route *filter, struct os_route *route);
};

EXPORT extern struct oonf_subsystem oonf_os_routing_subsystem;
EXPORT extern const struct os_route OS_ROUTE_WILDCARD;

/* prototypes for all os_routing functions */
EXPORT int os_routing_set(struct os_route *, bool set, bool del_similar);
EXPORT int os_routing_query(struct os_route *);
EXPORT void os_routing_interrupt(struct os_route *);

EXPORT const char *os_routing_to_string(
    struct os_route_str *buf, struct os_route *route);

#endif /* OS_ROUTING_H_ */

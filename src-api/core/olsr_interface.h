
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

#ifndef INTERFACE_H_
#define INTERFACE_H_

#ifndef _WIN32
#include <net/if.h>
#else
#define IF_NAMESIZE 16
#endif

#include "common/common_types.h"
#include "common/avl.h"
#include "common/list.h"
#include "common/netaddr.h"

#include "core/olsr_timer.h"

struct olsr_interface_data {
  /* Interface addresses with mesh-wide scope (at least) */
  struct netaddr if_v4, if_v6;

  /* IPv6 Interface address with global scope */
  struct netaddr linklocal_v6;

  /* mac address of interface */
  struct netaddr mac;

  /* interface name */
  char name[IF_NAMESIZE];

  /* interface index */
  unsigned index;

  /* true if the interface exists and is up */
  bool up;
};

struct olsr_interface {
  /* hook interfaces into tree */
  struct avl_node node;

  /* data of interface */
  struct olsr_interface_data data;

  /*
   * usage counter to allow multiple instances to add the same
   * interface
   */
  int usage_counter;

  /*
   * usage counter to keep track of the number of users on
   * this interface who want to send mesh traffic
   */
  int mesh_counter;

  /*
   * used to store internal state of interfaces before
   * configuring them for manet data forwarding.
   * Only used by os_specific code.
   */
  uint32_t _original_state;

  /* timer for lazy interface change handling */
  struct olsr_timer_entry change_timer;
};

struct olsr_interface_listener {
  /* hook into list of listeners */
  struct list_entity node;

  /* name of interface */
  const char *name;

  /*
   * set to true if listener is on a mesh traffic interface.
   * keep this false if in doubt, true will trigger some interface
   * reconfiguration to allow forwarding of user traffic
   */
  bool mesh;

  /* callback for interface change */
  void (*process)(struct olsr_interface_listener *, struct olsr_interface_data *old);

  /* will be set by the service before the process() function is called */
  struct olsr_interface *interface;
};

#define OLSR_FOR_ALL_INTERFACES(interf, ptr) avl_for_each_element_safe(&olsr_interface_tree, interf, node, ptr)
EXPORT extern struct avl_tree olsr_interface_tree;

EXPORT int olsr_interface_init(void) __attribute__((warn_unused_result));
EXPORT void olsr_interface_cleanup(void);

EXPORT int olsr_interface_add_listener(struct olsr_interface_listener *);
EXPORT void olsr_interface_remove_listener(struct olsr_interface_listener *);

EXPORT struct olsr_interface_data *olsr_interface_get_data(const char *name);
EXPORT void olsr_interface_trigger_change(const char *name, bool down);

#endif /* INTERFACE_H_ */

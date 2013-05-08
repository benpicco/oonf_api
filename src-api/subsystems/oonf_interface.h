
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

#ifndef INTERFACE_H_
#define INTERFACE_H_

#include <ifaddrs.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/list.h"
#include "common/netaddr.h"

#include "subsystems/oonf_timer.h"
#include "subsystems/os_net.h"

struct oonf_interface_listener {
  /* name of interface */
  const char *name;

  /*
   * set to true if listener is on a mesh traffic interface.
   * keep this false if in doubt, true will trigger some interface
   * reconfiguration to allow forwarding of user traffic
   */
  bool mesh;

  /* callback for interface change */
  void (*process)(struct oonf_interface_listener *);

  /*
   * pointer to the interface this listener is registered to, will be
   * set by the core while process() is called
   */
  struct oonf_interface *interface;

  /*
   * pointer to the interface data before the change happened, will be
   * set by the core while process() is called
   */
  struct oonf_interface_data *old;

  /* hook into list of listeners */
  struct list_entity _node;
};

EXPORT extern struct avl_tree oonf_interface_tree;
EXPORT extern struct oonf_subsystem oonf_interface_subsystem;

EXPORT int oonf_interface_add_listener(struct oonf_interface_listener *);
EXPORT void oonf_interface_remove_listener(struct oonf_interface_listener *);

EXPORT struct oonf_interface_data *oonf_interface_get_data(const char *name);
EXPORT void oonf_interface_trigger_change(const char *name, bool down);
EXPORT void oonf_interface_trigger_handler(struct oonf_interface *interf);

EXPORT int oonf_interface_find_address(struct netaddr *dst,
    struct netaddr *prefix, struct oonf_interface_data *);

#endif /* INTERFACE_H_ */

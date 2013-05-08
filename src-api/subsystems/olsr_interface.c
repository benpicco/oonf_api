
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

#include <netinet/in.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/list.h"
#include "common/netaddr.h"
#include "common/string.h"

#include "core/olsr_logging.h"
#include "core/olsr_subsystem.h"
#include "subsystems/olsr_interface.h"
#include "subsystems/olsr_timer.h"
#include "subsystems/os_net.h"
#include "subsystems/os_system.h"

/* timeinterval to delay change in interface to trigger actions */
#define OLSR_INTERFACE_CHANGE_INTERVAL 100

/* prototypes */
static int _init(void);
static void _cleanup(void);

static struct olsr_interface *_interface_add(const char *, bool mesh);
static void _interface_remove(struct olsr_interface *interf, bool mesh);
static void _cb_change_handler(void *);
static void _trigger_change_timer(struct olsr_interface *);

/* global tree of known interfaces */
struct avl_tree olsr_interface_tree;

/* subsystem definition */
struct oonf_subsystem oonf_interface_subsystem = {
  .init = _init,
  .cleanup = _cleanup,
};

static struct list_entity _interface_listener;
static struct olsr_timer_info _change_timer_info = {
  .name = "Interface change",
  .callback = _cb_change_handler,
};

static struct os_system_if_listener _iflistener = {
  .if_changed = olsr_interface_trigger_change,
};

/**
 * Initialize interface subsystem
 * @return always returns 0
 */
static int
_init(void) {
  olsr_timer_add(&_change_timer_info);

  avl_init(&olsr_interface_tree, avl_comp_strcasecmp, false);
  list_init_head(&_interface_listener);

  os_system_iflistener_add(&_iflistener);
  return 0;
}

/**
 * Cleanup interface subsystem
 */
static void
_cleanup(void) {
  struct olsr_interface_listener *listener, *l_it;

  list_for_each_element_safe(&_interface_listener, listener, _node, l_it) {
    olsr_interface_remove_listener(listener);
  }

  os_system_iflistener_remove(&_iflistener);
  olsr_timer_remove(&_change_timer_info);
}

/**
 * Add a listener to a specific interface
 * @param listener initialized listener object
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_interface_add_listener(
    struct olsr_interface_listener *listener) {
  if (list_is_node_added(&listener->_node)) {
    return 0;
  }

  if (listener->name) {
    listener->interface = _interface_add(listener->name, listener->mesh);
    if (listener->interface == NULL) {
      return -1;
    }
  }

  list_add_tail(&_interface_listener, &listener->_node);
  return 0;
}

/**
 * Removes a listener to an interface object
 * @param listener pointer to listener object
 */
void
olsr_interface_remove_listener(
    struct olsr_interface_listener *listener) {
  if (!list_is_node_added(&listener->_node)) {
    return;
  }

  if (listener->interface) {
    _interface_remove(listener->interface, listener->mesh);
  }

  list_remove(&listener->_node);
}

/**
 * Trigger a potential change in the interface settings. Normally
 * called by os_system code
 * @param name interface name
 * @param down true if interface is going down
 */
void
olsr_interface_trigger_change(const char *name, bool down) {
  struct olsr_interface *interf;

  OLSR_DEBUG(LOG_INTERFACE, "Change of interface %s was triggered", name);

  interf = avl_find_element(&olsr_interface_tree, name, interf, _node);
  if (interf == NULL) {
    return;
  }

  if (down) {
    interf->data.up = false;
  }

  olsr_interface_trigger_handler(interf);
}

/**
 * Trigger the interface change handler after a short waiting period
 * to accumulate multiple change events.
 * @param interf pointer to olsr interface
 */
void
olsr_interface_trigger_handler(struct olsr_interface *interf) {
  /* trigger interface reload in 100 ms */
  _trigger_change_timer(interf);
}

/**
 * @param name interface name
 * @return pointer to olsr interface data, NULL if not found
 */
struct olsr_interface_data *
olsr_interface_get_data(const char *name) {
  struct olsr_interface *interf;

  interf = avl_find_element(&olsr_interface_tree, name, interf, _node);
  if (interf == NULL) {
    return NULL;
  }

  return &interf->data;
}

/**
 * Find an IP address of an interface fitting to a specified prefix.
 * Destination will only be overwritten if address was found.
 * @param dst pointer to target address buffer
 * @param prefix specified prefix of address
 * @param ifdata interface data
 * @return 0 if an address was found, -1 otherwise
 */
int
olsr_interface_find_address(struct netaddr *dst,
    struct netaddr *prefix, struct olsr_interface_data *ifdata) {
  size_t i;

  for (i=0; i<ifdata->addrcount; i++) {
    if (netaddr_is_in_subnet(prefix, &ifdata->addresses[i])) {
      memcpy(dst, &ifdata->addresses[i], sizeof(*dst));
      return 0;
    }
  }
  return -1;
}

/**
 * Add an interface to the listener system
 * @param if_index index of interface
 * @param mesh true if interface is used for mesh traffic
 * @return pointer to interface struct, NULL if an error happened
 */
static struct olsr_interface *
_interface_add(const char *name, bool mesh) {
  struct olsr_interface *interf;

  interf = avl_find_element(&olsr_interface_tree, name, interf, _node);
  if (!interf) {
    /* allocate new interface */
    interf = calloc(1, sizeof(*interf));
    if (interf == NULL) {
      OLSR_WARN(LOG_INTERFACE, "Not enough memory for interface structure");
      return NULL;
    }

    /* hookup */
    strscpy(interf->data.name, name, sizeof(interf->data.name));
    interf->_node.key = interf->data.name;
    avl_insert(&olsr_interface_tree, &interf->_node);

    interf->data.index = if_nametoindex(name);

    interf->_change_timer.info = &_change_timer_info;
    interf->_change_timer.cb_context = interf;

    /* initialize data of interface */
    os_net_update_interface(&interf->data, name);
  }

  /* update reference counters */
  interf->usage_counter++;
  if(mesh) {
    if (interf->mesh_counter == 0) {
      os_net_init_mesh_if(interf);
    }
    interf->mesh_counter++;
  }

  /* trigger update */
  if (interf->usage_counter == 1) {
    /* new interface */
    _cb_change_handler(interf);
  }
  else {
    /* existing one, delay update */
    _trigger_change_timer(interf);
  }

  return interf;
}

/**
 * Remove an interface from the listener system. If multiple listeners
 * share an interface, this will only decrease the reference counter.
 * @param interf pointer to olsr_interface
 */
static void
_interface_remove(struct olsr_interface *interf, bool mesh) {
  /* handle mesh interface flag */
  if (mesh) {
    interf->mesh_counter--;

    if (interf->mesh_counter < 1) {
      /* no mesh interface anymore, remove routing settings */
      os_net_cleanup_mesh_if(interf);
    }
  }

  interf->usage_counter--;
  if (interf->usage_counter > 0) {
    return;
  }

  if (interf->data.addresses) {
    free(interf->data.addresses);
  }
  avl_remove(&olsr_interface_tree, &interf->_node);

  olsr_timer_stop(&interf->_change_timer);
  free(interf);
}

/**
 * Timer callback to handle potential change of data of an interface
 * @param ptr pointer to interface object
 */
static void
_cb_change_handler(void *ptr) {
  struct olsr_interface_data old_data, new_data;
  struct olsr_interface_listener *listener, *l_it;
  struct olsr_interface *interf;

  interf = ptr;

  OLSR_DEBUG(LOG_INTERFACE, "Change of interface %s in progress", interf->data.name);

  /* read interface data */
  memset(&new_data, 0, sizeof(new_data));
  if (os_net_update_interface(&new_data, interf->data.name)) {
    /* an error happened, try again */
    _trigger_change_timer(interf);
    return;
  }

  /* something changed ?
  if (memcmp(&interf->data, &new_data, sizeof(new_data)) == 0) {
    return;
  }
*/
  /* copy data to interface object, but remember the old data */
  memcpy(&old_data, &interf->data, sizeof(old_data));
  memcpy(&interf->data, &new_data, sizeof(interf->data));

  /* call listeners */
  list_for_each_element_safe(&_interface_listener, listener, _node, l_it) {
    if (listener->process != NULL
        && (listener->name == NULL
            || strcasecmp(listener->name, interf->data.name) == 0)) {
      listener->interface = interf;
      listener->old = &old_data;
      listener->process(listener);
      listener->interface = NULL;
      listener->old = NULL;
    }
  }

  if (old_data.addresses) {
    free (old_data.addresses);
  }
}

/**
 * Activate the change timer of an interface object
 * @param interf pointer to interface object
 */
static void
_trigger_change_timer(struct olsr_interface *interf) {
  olsr_timer_set(&interf->_change_timer, OLSR_INTERFACE_CHANGE_INTERVAL);
}

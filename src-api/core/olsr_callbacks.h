
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

#ifndef OLSR_CALLBACKS_H_
#define OLSR_CALLBACKS_H_

#include "common/common_types.h"
#include "common/list.h"
#include "common/avl.h"

enum olsr_callback_event {
  CALLBACK_EVENT_CHANGE,
  CALLBACK_EVENT_ADD,
  CALLBACK_EVENT_REMOVE,
};

struct olsr_callback_str {
  char buf[128];
};

struct olsr_callback_provider {
  /* name of callback provider */
  const char *name;

  /*
   * callback that convert pointer to object into char *,
   * which should contain the name/id of the object.
   */
  const char *(*cb_getkey)(struct olsr_callback_str *, void *);

  /* node for hooking this provider into the global tree */
  struct avl_node _node;

  /* list of consumers */
  struct list_entity _callbacks;

  /* protection against recursive _callbacks */
  bool _in_use;
};

struct olsr_callback_consumer {
  /* name of the consumer */
  const char *name;

  /* name of the provider */
  const char *provider;

  /* callback for 'cb_add object' event */
  void (*cb_add)(void *);

  /* callback for 'cb_change object' event */
  void (*cb_change)(void *);

  /* callback for 'cb_remove object' event */
  void (*cb_remove)(void *);

  /* node for hooking the consumer into the provider */
  struct list_entity _node;
};

#define OLSR_FOR_ALL_CALLBACK_PROVIDERS(provider, iterator) avl_for_each_element_safe(&olsr_callback_provider_tree, provider, _node, iterator)
#define OLSR_FOR_ALL_CALLBACK_CONSUMERS(provider, consumer, iterator) list_for_each_element_safe(&provider->_callbacks, consumer, _node, iterator)

EXPORT extern struct avl_tree olsr_callback_provider_tree;
EXPORT const char *OLSR_CALLBACK_EVENTS[3];

EXPORT void olsr_callback_init(void);
EXPORT void olsr_callback_cleanup(void);

EXPORT int olsr_callback_add(struct olsr_callback_provider *);
EXPORT void olsr_callback_remove(struct olsr_callback_provider *);

EXPORT int olsr_callback_register_consumer(struct olsr_callback_consumer *);
EXPORT void olsr_callback_unregister_consumer(struct olsr_callback_consumer *);

EXPORT void olsr_callback_event(struct olsr_callback_provider *, void *, enum olsr_callback_event);

/**
 * @param name callback provider name
 * @return pointer to callback provider, NULL if not found
 */
static INLINE struct olsr_callback_provider *
olsr_callback_get_provider(const char *name) {
  struct olsr_callback_provider *prv;
  return avl_find_element(&olsr_callback_provider_tree, name, prv, _node);
}

/**
 * @param buf pointer to callback provider buffer
 * @param prv pointer to callback provider
 * @param ptr pointer to callback provider object
 * @return name of object
 */
static INLINE const char *
olsr_callback_get_objectname(struct olsr_callback_str *buf,
    struct olsr_callback_provider *prv, void *ptr) {
  return prv->cb_getkey(buf, ptr);
}

#endif /* OLSR_CALLBACKS_H_ */

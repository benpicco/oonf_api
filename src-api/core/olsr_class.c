
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

#include <assert.h>
#include <stdlib.h>

#include "common/avl.h"
#include "common/avl_comp.h"
#include "core/olsr_class.h"
#include "core/olsr_logging.h"
#include "core/olsr_subsystem.h"

/* prototypes */
static void _free_freelist(struct olsr_class *);
static size_t _roundup(size_t);
static const char *_cb_to_keystring(struct olsr_objectkey_str *,
    struct olsr_class *, void *);

/* list of memory cookies */
struct avl_tree olsr_classes;

/* name of event types */
const char *OLSR_CLASS_EVENT_NAME[] = {
  [OLSR_OBJECT_ADDED] = "added",
  [OLSR_OBJECT_REMOVED] = "removed",
  [OLSR_OBJECT_CHANGED] = "changed",
};

/* remember if initialized or not */
OLSR_SUBSYSTEM_STATE(_memcookie_state);

/**
 * Initialize the memory cookie system
 */
void
olsr_class_init(void) {
  if (olsr_subsystem_init(&_memcookie_state))
    return;

  avl_init(&olsr_classes, avl_comp_strcasecmp, false, NULL);
}

/**
 * Cleanup the memory cookie system
 */
void
olsr_class_cleanup(void)
{
  struct olsr_class *info, *iterator;

  if (olsr_subsystem_cleanup(&_memcookie_state))
    return;

  /*
   * Walk the full index range and kill 'em all.
   */
  avl_for_each_element_safe(&olsr_classes, info, _node, iterator) {
    olsr_class_remove(info);
  }
}

/**
 * Allocate a new memcookie.
 * @param ci initialized memcookie
 */
void
olsr_class_add(struct olsr_class *ci)
{
  assert (ci->name);

  /* round up size to make block extendable */
  ci->size = _roundup(ci->size);

  /* hook into tree */
  ci->_node.key = ci->name;
  avl_insert(&olsr_classes, &ci->_node);

  /* add standard key generator if necessary */
  if (ci->to_keystring == NULL) {
    ci->to_keystring = _cb_to_keystring;
  }

  /* Init the free list */
  list_init_head(&ci->_free_list);

  /* Init the list for listeners */
  list_init_head(&ci->_listeners);
}

/**
 * Delete a memcookie and all memory in the free list
 * @param ci pointer to memcookie
 */
void
olsr_class_remove(struct olsr_class *ci)
{
  struct olsr_class_listener *l, *iterator;

  /* remove memcookie from tree */
  avl_remove(&olsr_classes, &ci->_node);

  /* remove all free memory blocks */
  _free_freelist(ci);

  /* remove all listeners */
  list_for_each_element_safe(&ci->_listeners, l, _node, iterator) {
    olsr_class_listener_remove(l);
  }
}

/**
 * Allocate a fixed amount of memory based on a passed in cookie type.
 * @param ci pointer to memcookie info
 * @return allocated memory
 */
void *
olsr_class_malloc(struct olsr_class *ci)
{
  struct list_entity *entity;
  void *ptr;

#if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_DEBUG
  bool reuse = false;
#endif

  /*
   * Check first if we have reusable memory.
   */
  if (list_is_empty(&ci->_free_list)) {
    /*
     * No reusable memory block on the free_list.
     * Allocate a fresh one.
     */
    ptr = calloc(1, ci->size);
    if (ptr == NULL) {
      OLSR_WARN(LOG_CLASS, "Out of memory for: %s", ci->name);
      return NULL;
    }
    ci->_allocated++;
  } else {
    /*
     * There is a memory block on the free list.
     * Carve it out of the list, and clean.
     */
    entity = ci->_free_list.next;
    list_remove(entity);

    memset(entity, 0, ci->size);
    ptr = entity;

    ci->_free_list_size--;
    ci->_recycled++;
#if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_DEBUG
    reuse = true;
#endif
  }

  /* Stats keeping */
  ci->_current_usage++;

  OLSR_DEBUG(LOG_CLASS, "MEMORY: alloc %s, %" PRINTF_SIZE_T_SPECIFIER " bytes%s\n",
             ci->name, ci->size, reuse ? ", reuse" : "");
  return ptr;
}

/**
 * Free a memory block owned by a given cookie.
 * @param ci pointer to memcookie info
 * @param ptr pointer to memory block
 */
void
olsr_class_free(struct olsr_class *ci, void *ptr)
{
  struct list_entity *item;
#if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_DEBUG
  bool reuse = false;
#endif

  /*
   * Rather than freeing the memory right away, try to reuse at a later
   * point. Keep at least ten percent of the active used blocks or at least
   * ten blocks on the free list.
   */
  if (ci->_free_list_size < ci->min_free_count
      || (ci->_free_list_size < ci->_current_usage / OLSR_CLASS_FREE_THRESHOLD)) {
    item = ptr;

    list_add_tail(&ci->_free_list, item);

    ci->_free_list_size++;
#if OONF_LOGGING_LEVEL >= OONF_LOGGING_LEVEL_DEBUG
    reuse = true;
#endif
  } else {

    /* No interest in reusing memory. */
    free(ptr);
  }

  /* Stats keeping */
  ci->_current_usage--;

  OLSR_DEBUG(LOG_CLASS, "MEMORY: free %s, %"PRINTF_SIZE_T_SPECIFIER" bytes%s\n",
             ci->name, ci->size, reuse ? ", reuse" : "");
}

/**
 * Register an extension to an existing class without objects.
 * Extensions can NOT be unregistered!
 * @param ext pointer to class extension
 * @return 0 if extension was registered, -1 if an error happened
 */
int
olsr_class_extend(struct olsr_class_extension *ext) {
  struct olsr_class *c;

  c = avl_find_element(&olsr_classes, ext->class_name, c, _node);
  if (c == NULL) {
    OLSR_WARN(LOG_CLASS, "Unknown class %s for extension %s",
        ext->name, ext->class_name);
    return -1;
  }

  if (c->_allocated != 0) {
    OLSR_WARN(LOG_CLASS, "Class %s is already in use and cannot be extended",
        c->name);
    return -1;
  }

  /* make sure freelist is empty */
  _free_freelist(c);

  /* old size is new offset */
  ext->_offset = c->size;

  /* calculate new size */
  c->size = _roundup(c->size + ext->size);
  return 0;
}

/**
 * Add a listener to an OLSR class
 * @param l pointer o listener
 * @return 0 if successful, -1 otherwise
 */
int
olsr_class_listener_add(struct olsr_class_listener *l) {
  struct olsr_class *c;

  c = avl_find_element(&olsr_classes, l->class_name, c, _node);
  if (c == NULL) {
    OLSR_WARN(LOG_CLASS, "Unknown class %s for listener %s",
        l->name, l->class_name);
    return -1;
  }

  /* hook listener into class */
  list_add_tail(&c->_listeners, &l->_node);
  return 0;
}

/**
 * Remove listener from class
 * @param l pointer to listener
 */
void
olsr_class_listener_remove(struct olsr_class_listener *l) {
  list_remove(&l->_node);
}

/**
 * Fire an event for a class
 * @param c pointer to class
 * @param ptr pointer to object
 * @param evt type of event
 */
void
olsr_class_event(struct olsr_class *c, void *ptr, enum olsr_class_event evt) {
  struct olsr_class_listener *l;
  struct olsr_objectkey_str buf;

  OLSR_DEBUG(LOG_CLASS, "Fire '%s' event for %s",
      OLSR_CLASS_EVENT_NAME[evt], c->to_keystring(&buf, c, ptr));
  list_for_each_element(&c->_listeners, l, _node) {
    if (evt == OLSR_OBJECT_ADDED && l->cb_add != NULL) {
      OLSR_DEBUG(LOG_CLASS, "Fire listener %s", l->name);
      l->cb_add(ptr);
    }
    else if (evt == OLSR_OBJECT_REMOVED && l->cb_remove != NULL) {
      OLSR_DEBUG(LOG_CLASS, "Fire listener %s", l->name);
      l->cb_remove(ptr);
    }
    else if (evt == OLSR_OBJECT_CHANGED && l->cb_change != NULL) {
      OLSR_DEBUG(LOG_CLASS, "Fire listener %s", l->name);
      l->cb_change(ptr);
    }
  }
  OLSR_DEBUG(LOG_CLASS, "Fire event finished");
}

/**
 * @param size memory size in byte
 * @return rounded up size to sizeof(struct list_entity)
 */
static size_t
_roundup(size_t size) {
  size = size + sizeof(struct list_entity) - 1;
  size = size & (~(sizeof(struct list_entity) - 1));

  return size;
}

/**
 * Free all objects in the free_list of a memory cookie
 * @param ci pointer to memory cookie
 */
static void
_free_freelist(struct olsr_class *ci) {
  while (!list_is_empty(&ci->_free_list)) {
    struct list_entity *item;
    item = ci->_free_list.next;

    list_remove(item);
    free(item);
  }
  ci->_free_list_size = 0;
}

/**
 * Default keystring creator
 * @param buf pointer to target buffer
 * @param class olsr class
 * @param ptr pointer to object
 * @return pointer to target buffer
 */
static const char *
_cb_to_keystring(struct olsr_objectkey_str *buf,
    struct olsr_class *class, void *ptr) {
  snprintf(buf->buf, sizeof(*buf), "%s::0x%"PRINTF_SIZE_T_SPECIFIER"x",
      class->name, (size_t)ptr);

  return buf->buf;
}

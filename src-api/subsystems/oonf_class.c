
/*
 * The olsr.org Optimized Link-State Routing daemon version 2 (olsrd2)
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
#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static void _free_freelist(struct oonf_class *);
static size_t _roundup(size_t);
static const char *_cb_to_keystring(struct oonf_objectkey_str *,
    struct oonf_class *, void *);

/* list of memory cookies */
struct avl_tree oonf_classes;

/* name of event types */
const char *OONF_CLASS_EVENT_NAME[] = {
  [OONF_OBJECT_ADDED] = "added",
  [OONF_OBJECT_REMOVED] = "removed",
  [OONF_OBJECT_CHANGED] = "changed",
};

/* subsystem definition */
struct oonf_subsystem oonf_class_subsystem = {
  .name = "class",
  .init = _init,
  .cleanup = _cleanup,
};

/**
 * Initialize the class system
 * @return always returns 0
 */
static int
_init(void) {
  avl_init(&oonf_classes, avl_comp_strcasecmp, false);
  return 0;
}

/**
 * Cleanup the memory cookie system
 */
static void
_cleanup(void)
{
  struct oonf_class *info, *iterator;

  /*
   * Walk the full index range and kill 'em all.
   */
  avl_for_each_element_safe(&oonf_classes, info, _node, iterator) {
    oonf_class_remove(info);
  }
}

/**
 * Allocate a new memcookie.
 * @param ci initialized memcookie
 */
void
oonf_class_add(struct oonf_class *ci)
{
  assert (ci->name);

  /* round up size to make block extendable */
  ci->total_size = _roundup(ci->size);

  /* hook into tree */
  ci->_node.key = ci->name;
  avl_insert(&oonf_classes, &ci->_node);

  /* add standard key generator if necessary */
  if (ci->to_keystring == NULL) {
    ci->to_keystring = _cb_to_keystring;
  }

  /* Init list heads */
  list_init_head(&ci->_free_list);
  list_init_head(&ci->_extensions);

  OONF_DEBUG(LOG_CLASS, "Class %s added: %" PRINTF_SIZE_T_SPECIFIER " bytes\n",
             ci->name, ci->total_size);
}

/**
 * Delete a memcookie and all memory in the free list
 * @param ci pointer to memcookie
 */
void
oonf_class_remove(struct oonf_class *ci)
{
  struct oonf_class_extension *ext, *iterator;

  /* remove memcookie from tree */
  avl_remove(&oonf_classes, &ci->_node);

  /* remove all free memory blocks */
  _free_freelist(ci);

  /* remove all listeners */
  list_for_each_element_safe(&ci->_extensions, ext, _node, iterator) {
    oonf_class_extension_remove(ext);
  }

  OONF_DEBUG(LOG_CLASS, "Class %s removed\n", ci->name);
}

/**
 * Set a class to a new size. Can only be done as long as no
 * memory objects are allocated.
 * @param ci pointer to class
 */
int
oonf_class_resize(struct oonf_class *ci) {
  struct oonf_class_extension *ext;
  if (ci->_current_usage > 0) {
    return -1;
  }

  /* round up size to make block extendable */
  ci->total_size = _roundup(ci->size);

  _free_freelist(ci);

  /* recalculate offsets */
  list_for_each_element(&ci->_extensions, ext, _node) {
    ext->_offset = ci->total_size;
    OONF_DEBUG(LOG_CLASS, "Class %s resized: "
        " '%s' has offset %" PRINTF_SIZE_T_SPECIFIER " and size %" PRINTF_SIZE_T_SPECIFIER "\n",
               ci->name, ext->ext_name, ext->_offset, ext->size);

    ci->total_size = _roundup(ci->total_size + ext->size);
  }

  OONF_DEBUG(LOG_CLASS, "Class %s: resized to %" PRINTF_SIZE_T_SPECIFIER " bytes\n",
             ci->name, ci->total_size);

  return 0;
}

/**
 * Allocate a fixed amount of memory based on a passed in cookie type.
 * @param ci pointer to memcookie info
 * @return allocated memory
 */
void *
oonf_class_malloc(struct oonf_class *ci)
{
  struct list_entity *entity;
  void *ptr;

#ifdef OONF_LOG_DEBUG_INFO
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
    ptr = calloc(1, ci->total_size);
    if (ptr == NULL) {
      OONF_WARN(LOG_CLASS, "Out of memory for: %s", ci->name);
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

    memset(entity, 0, ci->total_size);
    ptr = entity;

    ci->_free_list_size--;
    ci->_recycled++;
#ifdef OONF_LOG_DEBUG_INFO
    reuse = true;
#endif
  }

  /* Stats keeping */
  ci->_current_usage++;

  OONF_DEBUG(LOG_CLASS, "MEMORY: alloc %s, %" PRINTF_SIZE_T_SPECIFIER " bytes%s\n",
             ci->name, ci->total_size, reuse ? ", reuse" : "");
  return ptr;
}

/**
 * Free a memory block owned by a given cookie.
 * @param ci pointer to memcookie info
 * @param ptr pointer to memory block
 */
void
oonf_class_free(struct oonf_class *ci, void *ptr)
{
  struct list_entity *item;
#ifdef OONF_LOG_DEBUG_INFO
  bool reuse = false;
#endif

  /*
   * Rather than freeing the memory right away, try to reuse at a later
   * point. Keep at least ten percent of the active used blocks or at least
   * ten blocks on the free list.
   */
  if (ci->_free_list_size < ci->min_free_count
      || (ci->_free_list_size < ci->_current_usage / OONF_CLASS_FREE_THRESHOLD)) {
    item = ptr;

    list_add_tail(&ci->_free_list, item);

    ci->_free_list_size++;
#ifdef OONF_LOG_DEBUG_INFO
    reuse = true;
#endif
  } else {

    /* No interest in reusing memory. */
    free(ptr);
  }

  /* Stats keeping */
  ci->_current_usage--;

  OONF_DEBUG(LOG_CLASS, "MEMORY: free %s, %"PRINTF_SIZE_T_SPECIFIER" bytes%s\n",
             ci->name, ci->size, reuse ? ", reuse" : "");
}

/**
 * Register an extension to an existing class without objects.
 * This function can only fail if ext->size is not 0.
 * @param ext pointer to class extension
 * @return 0 if extension was registered, -1 if an error happened
 */
int
oonf_class_extension_add(struct oonf_class_extension *ext) {
  struct oonf_class *c;

  if (oonf_class_is_extension_registered(ext)) {
    /* already registered */
    return 0;
  }

  c = avl_find_element(&oonf_classes, ext->class_name, c, _node);
  if (c == NULL) {
    OONF_WARN(LOG_CLASS, "Unknown class %s for extension %s",
        ext->class_name, ext->ext_name);
    return -1;
  }

  if (c->_allocated != 0 && ext->size > 0) {
    OONF_WARN(LOG_CLASS, "Class %s is already in use and cannot be extended",
        c->name);
    return -1;
  }

  /* add to class extension list */
  list_add_tail(&c->_extensions, &ext->_node);

  if (ext->size > 0) {
    /* make sure freelist is empty */
    _free_freelist(c);

    /* old size is new offset */
    ext->_offset = c->total_size;

    /* calculate new size */
    c->total_size = _roundup(c->total_size + ext->size);

    OONF_DEBUG(LOG_CLASS, "Class %s extended: %" PRINTF_SIZE_T_SPECIFIER " bytes,"
        " '%s' has offset %" PRINTF_SIZE_T_SPECIFIER " and length %" PRINTF_SIZE_T_SPECIFIER "\n",
        c->name, c->total_size, ext->ext_name, ext->_offset, ext->size);
  }

  return 0;
}

/**
 * Remove listener from class
 * @param l pointer to listener
 */
void
oonf_class_extension_remove(struct oonf_class_extension *ext) {
  if (list_is_node_added(&ext->_node)) {
    list_remove(&ext->_node);
    ext->_offset = 0;
  }
}

/**
 * Fire an event for a class
 * @param c pointer to class
 * @param ptr pointer to object
 * @param evt type of event
 */
void
oonf_class_event(struct oonf_class *c, void *ptr, enum oonf_class_event evt) {
  struct oonf_class_extension *ext;
#ifdef OONF_LOG_DEBUG_INFO
  struct oonf_objectkey_str buf;
#endif

  OONF_DEBUG(LOG_CLASS, "Fire '%s' event for %s",
      OONF_CLASS_EVENT_NAME[evt], c->to_keystring(&buf, c, ptr));
  list_for_each_element(&c->_extensions, ext, _node) {
    if (evt == OONF_OBJECT_ADDED && ext->cb_add != NULL) {
      OONF_DEBUG(LOG_CLASS, "Fire listener %s", ext->ext_name);
      ext->cb_add(ptr);
    }
    else if (evt == OONF_OBJECT_REMOVED && ext->cb_remove != NULL) {
      OONF_DEBUG(LOG_CLASS, "Fire listener %s", ext->ext_name);
      ext->cb_remove(ptr);
    }
    else if (evt == OONF_OBJECT_CHANGED && ext->cb_change != NULL) {
      OONF_DEBUG(LOG_CLASS, "Fire listener %s", ext->ext_name);
      ext->cb_change(ptr);
    }
  }
  OONF_DEBUG(LOG_CLASS, "Fire event finished");
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
_free_freelist(struct oonf_class *ci) {
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
_cb_to_keystring(struct oonf_objectkey_str *buf,
    struct oonf_class *class, void *ptr) {
  snprintf(buf->buf, sizeof(*buf), "%s::0x%"PRINTF_SIZE_T_HEX_SPECIFIER,
      class->name, (size_t)ptr);

  return buf->buf;
}

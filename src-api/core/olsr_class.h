
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

#ifndef _OLSR_CLASS_H
#define _OLSR_CLASS_H

#include "common/common_types.h"
#include "common/list.h"
#include "common/avl.h"

enum olsr_class_event {
  OLSR_OBJECT_CHANGED,
  OLSR_OBJECT_ADDED,
  OLSR_OBJECT_REMOVED,
};

struct olsr_objectkey_str {
  char buf[128];
};

/*
 * This structure represents a class of memory object, each with the same size.
 */
struct olsr_class {
  /* Name of class */
  const char *name;

  /* Size of memory blocks */
  size_t size;

  /*
   * minimum number of chunks the allocator will keep
   * in the free list before starting to deallocate one
   */
  uint32_t min_free_count;

  /*
   * function pointer that converts a pointer to the object into a
   * human readable key
   */
  const char *(*to_keystring)(struct olsr_objectkey_str *, struct olsr_class *, void *);

  /* Size of class including extensions */
  size_t total_size;

  /* List node for classes */
  struct avl_node _node;

  /* List head for recyclable blocks */
  struct list_entity _free_list;

  /* listeners of this class */
  struct list_entity _listeners;

  /* extensions of this class */
  struct list_entity _extensions;

  /* Length of free list */
  uint32_t _free_list_size;

  /* Stats, resource usage */
  uint32_t _current_usage;

  /* Stats, allocated/recycled memory blocks */
  uint32_t _allocated, _recycled;
};

/*
 * This structure represents an extension of a class. The extension can
 * be registered as long as no memory objects are allocated
 */
struct olsr_class_extension {
  /* name of the extension for logging/debug purpose */
  const char *name;

  /* name of the class to be extended */
  const char *class_name;

  /* size of the extension */
  size_t size;

  /* offset of the extension within the memory block */
  size_t _offset;

  /* node for list of class extensions */
  struct list_entity _node;
};

struct olsr_class_listener {
  /* name of the consumer */
  const char *name;

  /* name of the provider */
  const char *class_name;

  /* callback for 'cb_add object' event */
  void (*cb_add)(void *);

  /* callback for 'cb_change object' event */
  void (*cb_change)(void *);

  /* callback for 'cb_remove object' event */
  void (*cb_remove)(void *);

  /* node for hooking the consumer into the provider */
  struct list_entity _node;
};

/* percentage of blocks kept in the free list compared to allocated blocks */
#define OLSR_CLASS_FREE_THRESHOLD 10   /* Blocks / Percent  */

EXPORT extern struct avl_tree olsr_classes;
EXPORT extern const char *OLSR_CLASS_EVENT_NAME[];

/* Externals. */
void olsr_class_init(void);
void olsr_class_cleanup(void);

EXPORT void olsr_class_add(struct olsr_class *);
EXPORT void olsr_class_remove(struct olsr_class *);
EXPORT int olsr_class_resize(struct olsr_class *)
    __attribute__((warn_unused_result));

EXPORT void *olsr_class_malloc(struct olsr_class *)
    __attribute__((warn_unused_result));
EXPORT void olsr_class_free(struct olsr_class *, void *);

EXPORT int olsr_class_extend(struct olsr_class_extension *);

EXPORT int olsr_class_listener_add(struct olsr_class_listener *);
EXPORT void olsr_class_listener_remove(struct olsr_class_listener *);

EXPORT void olsr_class_event(struct olsr_class *, void *, enum olsr_class_event);

/**
 * @param ci pointer to class
 * @return number of blocks currently in use
 */
static INLINE uint32_t
olsr_class_get_usage(struct olsr_class *ci) {
  return ci->_current_usage;
}

/**
 * @param ci pointer to class
 * @return number of blocks currently in free list
 */
static INLINE uint32_t
olsr_class_get_free(struct olsr_class *ci) {
  return ci->_free_list_size;
}

/**
 * @param ci pointer to class
 * @return total number of allocations during runtime
 */
static INLINE uint32_t
olsr_class_get_allocations(struct olsr_class *ci) {
  return ci->_allocated;
}

/**
 * @param ci pointer to class
 * @return total number of allocations during runtime
 */
static INLINE uint32_t
olsr_class_get_recycled(struct olsr_class *ci) {
  return ci->_recycled;
}

/**
 * @param ext extension data structure
 * @param ptr pointer to base block
 * @return pointer to extensions memory block
 */
static INLINE void *
olsr_class_get_extension(struct olsr_class_extension *ext, void *ptr) {
  return ((char *)ptr) + ext->_offset;
}

/**
 * @param ext pointer to class extension
 * @return true if extension is registered
 */
static INLINE bool
olsr_class_is_extension_registered(struct olsr_class_extension *ext) {
  return ext->_offset > 0;
}

#endif /* _OLSR_CLASS_H */

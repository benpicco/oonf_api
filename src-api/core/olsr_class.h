
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

#ifndef _OLSR_MEMCOOKIE_H
#define _OLSR_MEMCOOKIE_H

#include "common/common_types.h"
#include "common/list.h"
#include "common/avl.h"

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

  /* Statistics and internal bookkeeping */

  /* List node for classes */
  struct list_entity _node;

  /* List head for recyclable blocks */
  struct list_entity _free_list;

  /* Length of free list */
  uint32_t _free_list_size;

  /* Stats, resource usage */
  uint32_t _current_usage;

  /* Stats, allocated/recycled memory blocks */
  uint32_t _allocated, _recycled;
};

struct olsr_class_extension {
  size_t size;

  size_t _offset;
};

/* percentage of blocks kept in the free list compared to allocated blocks */
#define OLSR_CLASS_FREE_THRESHOLD 10   /* Blocks / Percent  */

EXPORT extern struct list_entity olsr_classes;

/* Externals. */
EXPORT void olsr_class_init(void);
EXPORT void olsr_class_cleanup(void);

EXPORT void olsr_class_add(struct olsr_class *);
EXPORT void olsr_class_remove(struct olsr_class *);

EXPORT void *olsr_class_malloc(struct olsr_class *)
    __attribute__((warn_unused_result));
EXPORT void olsr_class_free(struct olsr_class *, void *);

EXPORT int olsr_class_extend(
    struct olsr_class *, struct olsr_class_extension *);

/**
 * @param ci pointer to memcookie info
 * @return number of blocks currently in use
 */
static INLINE uint32_t
olsr_class_get_usage(struct olsr_class *ci) {
  return ci->_current_usage;
}

/**
 * @param ci pointer to memcookie info
 * @return number of blocks currently in free list
 */
static INLINE uint32_t
olsr_class_get_free(struct olsr_class *ci) {
  return ci->_free_list_size;
}

/**
 * @param ci pointer to memcookie info
 * @return total number of allocations during runtime
 */
static INLINE uint32_t
olsr_class_get_allocations(struct olsr_class *ci) {
  return ci->_allocated;
}

/**
 * @param ci pointer to memcookie info
 * @return total number of allocations during runtime
 */
static INLINE uint32_t
olsr_class_get_recycled(struct olsr_class *ci) {
  return ci->_recycled;
}

/**
 * @param ptr pointer to base block
 * @param ext extension data structure
 * @return pointer to extensions memory block
 */
static INLINE void *
olsr_class_get_extension(void *ptr, struct olsr_class_extension *ext) {
  return ((char *)ptr) + ext->_offset;
}

#endif /* _OLSR_MEMCOOKIE_H */

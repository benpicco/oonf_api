
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

#ifndef _OONF_SCHEDULER
#define _OONF_SCHEDULER

#include "common/common_types.h"
#include "common/list.h"
#include "common/avl.h"

/* prototype for socket handler */
typedef void (*socket_handler_func) (int fd, void *data,
    bool event_read, bool event_write);

/* This struct represents a single registered socket handler */
struct oonf_socket_entry {
  /* file descriptor of the socket */
  int fd;

  /* socket handler */
  socket_handler_func process;

  /* custom data pointer for sockets */
  void *data;

  /* event mask for socket handler */
  bool event_read, event_write;

  /* list of socket handlers */
  struct list_entity _node;
};

#define LOG_SOCKET oonf_socket_subsystem.logging
EXPORT extern struct oonf_subsystem oonf_socket_subsystem;
EXPORT extern struct list_entity oonf_socket_head;

EXPORT int oonf_socket_handle(bool (*stop_scheduler)(void), uint64_t) __attribute__((warn_unused_result));

EXPORT void oonf_socket_add(struct oonf_socket_entry *);
EXPORT void oonf_socket_remove(struct oonf_socket_entry *);

/**
 * Enable one or both flags of a socket handler
 * @param sock pointer to socket entry
 */
static INLINE void
oonf_socket_set_read(struct oonf_socket_entry *entry, bool event_read)
{
  entry->event_read = event_read;
}

/**
 * Disable one or both flags of a socket handler
 * @param sock pointer to socket entry
 */
static INLINE void
oonf_socket_set_write(struct oonf_socket_entry *entry, bool event_write)
{
  entry->event_write = event_write;
}

#endif


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

#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common/avl.h"
#include "common/avl_comp.h"
#include "subsystems/oonf_clock.h"
#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/os_net.h"
#include "subsystems/oonf_socket.h"
#include "subsystems/oonf_timer.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

/* List of all active sockets in scheduler */
struct list_entity socket_head;

/* subsystem definition */
struct oonf_subsystem oonf_socket_subsystem = {
  .init = _init,
  .cleanup = _cleanup,
};

/**
 * Initialize olsr socket scheduler
 * @return always returns 0
 */
static int
_init(void) {
  list_init_head(&socket_head);
  return 0;
}

/**
 * Cleanup olsr socket scheduler.
 * This will close and free all sockets.
 */
static void
_cleanup(void)
{
  struct oonf_socket_entry *entry, *iterator;

  OONF_FOR_ALL_SOCKETS(entry, iterator) {
    list_remove(&entry->node);
    os_close(entry->fd);
  }
}

/**
 * Add a socket handler to the scheduler
 *
 * @param entry pointer to initialized socket entry
 * @return -1 if an error happened, 0 otherwise
 */
void
oonf_socket_add(struct oonf_socket_entry *entry)
{
  assert (entry->fd >= 0);
  assert (entry->process);

  OONF_DEBUG(LOG_SOCKET, "Adding socket entry %d to scheduler\n", entry->fd);

  list_add_before(&socket_head, &entry->node);
}

/**
 * Remove a socket from the socket scheduler
 * @param entry pointer to socket entry
 */
void
oonf_socket_remove(struct oonf_socket_entry *entry)
{
  OONF_DEBUG(LOG_SOCKET, "Removing socket entry %d\n", entry->fd);

  list_remove(&entry->node);
}

/**
 * Handle all incoming socket events until a certain time
 * @param stop_scheduler pointer to a callback function that tells
 *   the scheduler if it should return to the mainloop. Might be NULL.
 * @param stop_time timestamp when the handler should stop,
 *   0 if it should keep running
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_socket_handle(bool (*stop_scheduler)(void), uint64_t stop_time)
{
  struct oonf_socket_entry *entry, *iterator;
  uint64_t next_event;
  struct timeval tv, *tv_ptr;
  int n = 0;
  bool fd_read;
  bool fd_write;

  if (stop_time == 0) {
    stop_time = ~0ull;
  }

  while (true) {
    fd_set ibits, obits;
    int hfd = 0;

    /* Update time since this is much used by the parsing functions */
    if (oonf_clock_update()) {
      return -1;
    }

    if (oonf_clock_getNow() >= stop_time) {
      return 0;
    }

    oonf_timer_walk();

    if (stop_scheduler != NULL && stop_scheduler()) {
      return 0;
    }

    /* no event left for now, prepare for select () */
    fd_read = false;
    fd_write = false;

    FD_ZERO(&ibits);
    FD_ZERO(&obits);

    /* Adding file-descriptors to FD set */
    OONF_FOR_ALL_SOCKETS(entry, iterator) {
      if (entry->process == NULL) {
        continue;
      }

      if (entry->event_read) {
        fd_read = true;
        FD_SET((unsigned int)entry->fd, &ibits);        /* And we cast here since we get a warning on Win32 */
      }
      if (entry->event_write) {
        fd_write = true;
        FD_SET((unsigned int)entry->fd, &obits);        /* And we cast here since we get a warning on Win32 */
      }
      if ((entry->event_read || entry->event_write) != 0 && entry->fd >= hfd) {
        hfd = entry->fd + 1;
      }
    }

    next_event = oonf_timer_getNextEvent();
    if (next_event > stop_time) {
      next_event = stop_time;
    }

    if (next_event == ~0ull) {
      /* no events waiting */
      tv_ptr = NULL;
    }
    else {
      /* convert time interval until event triggers */
      next_event = oonf_clock_get_relative(next_event);

      tv_ptr = &tv;
      tv.tv_sec = (time_t)(next_event / 1000ull);
      tv.tv_usec = (int)(next_event % 1000) * 1000;
    }

    do {
      if (stop_scheduler != NULL && stop_scheduler()) {
        return 0;
      }
      n = os_select(hfd,
          fd_read ? &ibits : NULL,
          fd_write ? &obits : NULL,
          NULL, tv_ptr);
    } while (n == -1 && errno == EINTR);

    if (n == 0) {               /* timeout! */
      break;
    }
    if (n < 0) {              /* Did something go wrong? */
      OONF_WARN(LOG_SOCKET, "select error: %s (%d)", strerror(errno), errno);
      return -1;
    }

    /* Update time since this is much used by the parsing functions */
    if (oonf_clock_update()) {
      return -1;
    }
    OONF_FOR_ALL_SOCKETS(entry, iterator) {
      if (entry->process == NULL) {
        continue;
      }

      fd_read = FD_ISSET(entry->fd, &ibits) != 0;
      fd_write = FD_ISSET(entry->fd, &obits) != 0;
      if (fd_read || fd_write) {
        entry->process(entry->fd, entry->data, fd_read, fd_write);
      }
    }
  }
  return 0;
}

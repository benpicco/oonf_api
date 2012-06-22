
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

#include <unistd.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "common/avl.h"
#include "common/avl_comp.h"
#include "core/olsr_clock.h"
#include "core/olsr_logging.h"
#include "core/os_net.h"
#include "core/olsr_socket.h"
#include "core/olsr_timer.h"
#include "core/olsr_subsystem.h"

/* List of all active sockets in scheduler */
struct list_entity socket_head;

static bool _stop_scheduler;

/* remember if initialized or not */
OLSR_SUBSYSTEM_STATE(_socket_state);

/**
 * Initialize olsr socket scheduler
 */
void
olsr_socket_init(void) {
  if (olsr_subsystem_init(&_socket_state))
    return;

  list_init_head(&socket_head);
}

/**
 * Cleanup olsr socket scheduler.
 * This will close and free all sockets.
 */
void
olsr_socket_cleanup(void)
{
  struct olsr_socket_entry *entry, *iterator;

  if (olsr_subsystem_cleanup(&_socket_state))
    return;

  OLSR_FOR_ALL_SOCKETS(entry, iterator) {
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
olsr_socket_add(struct olsr_socket_entry *entry)
{
  assert (entry->fd >= 0);
  assert (entry->process);

  OLSR_DEBUG(LOG_SOCKET, "Adding socket entry %d to scheduler\n", entry->fd);

  list_add_before(&socket_head, &entry->node);
}

/**
 * Remove a socket from the socket scheduler
 * @param entry pointer to socket entry
 */
void
olsr_socket_remove(struct olsr_socket_entry *entry)
{
  OLSR_DEBUG(LOG_SOCKET, "Removing socket entry %d\n", entry->fd);

  list_remove(&entry->node);
}

/**
 * Prevent the scheduler from waiting until timeout.
 */
void
olsr_socket_stop_scheduler(void) {
  _stop_scheduler = true;
}

/**
 * Handle all incoming socket events until a certain time
 * @param stop_time timestamp when the handler should stop,
 *   0 if it should keep running
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_socket_handle(uint64_t stop_time)
{
  struct olsr_socket_entry *entry, *iterator;
  uint64_t next_event;
  struct timeval tv, *tv_ptr;
  int n = 0;
  bool fd_read;
  bool fd_write;

  if (stop_time == 0) {
    stop_time = ~0ull;
  }

  _stop_scheduler = false;

  while (true) {
    fd_set ibits, obits;
    int hfd = 0;

    /* Update time since this is much used by the parsing functions */
    if (olsr_clock_update()) {
      return -1;
    }

    if (olsr_clock_getNow() >= stop_time) {
      return 0;
    }

    if (olsr_timer_getNextEvent() <= olsr_clock_getNow()) {
      olsr_timer_walk();
    }

    if (_stop_scheduler) {
      return 0;
    }

    /* no event left for now, prepare for select () */
    fd_read = false;
    fd_write = false;

    FD_ZERO(&ibits);
    FD_ZERO(&obits);

    /* Adding file-descriptors to FD set */
    OLSR_FOR_ALL_SOCKETS(entry, iterator) {
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

    next_event = olsr_timer_getNextEvent();
    if (next_event > stop_time) {
      next_event = stop_time;
    }

    if (next_event == ~0ull) {
      /* no events waiting */
      tv_ptr = NULL;
    }
    else {
      /* convert time interval until event triggers */
      next_event = olsr_clock_get_relative(next_event);

      tv_ptr = &tv;
      tv.tv_sec = (time_t)(next_event / 1000ull);
      tv.tv_usec = (int)(next_event % 1000) * 1000;
    }

    do {
      if (_stop_scheduler) {
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
      OLSR_WARN(LOG_SOCKET, "select error: %s (%d)", strerror(errno), errno);
      return -1;
    }

    /* Update time since this is much used by the parsing functions */
    if (olsr_clock_update()) {
      return -1;
    }
    OLSR_FOR_ALL_SOCKETS(entry, iterator) {
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

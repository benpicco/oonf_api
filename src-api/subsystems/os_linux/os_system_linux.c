
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

/* must be first because of a problem with linux/rtnetlink.h */
#include <sys/socket.h>

/* and now the rest of the includes */
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/rtnetlink.h>
#include <linux/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <unistd.h>
#include <sys/time.h>
#include <time.h>

#include "common/common_types.h"
#include "common/string.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_socket.h"
#include "subsystems/os_system.h"

#ifndef SOL_NETLINK
#define SOL_NETLINK 270
#endif

#define OS_SYSTEM_NETLINK_TIMEOUT 100

/* prototypes */
static int _init(void);
static void _cleanup(void);

static void _cb_handle_netlink_timeout(void *);
static void _netlink_handler(int fd, void *data,
    bool event_read, bool event_write);
static void _handle_rtnetlink(struct nlmsghdr *hdr);

static void _handle_nl_err(struct os_system_netlink *, struct nlmsghdr *);

/* ioctl socket */
static int _ioctl_fd = -1;

/* static buffers for receiving/sending a netlink message */
static struct sockaddr_nl _netlink_nladdr = {
  .nl_family = AF_NETLINK
};

static struct iovec _netlink_rcv_iov;
static struct msghdr _netlink_rcv_msg = {
  &_netlink_nladdr,
  sizeof(_netlink_nladdr),
  &_netlink_rcv_iov,
  1,
  NULL,
  0,
  0
};

static struct nlmsghdr _netlink_hdr_done = {
  .nlmsg_len = sizeof(struct nlmsghdr),
  .nlmsg_type = NLMSG_DONE
};

static struct iovec _netlink_send_iov[2] = {
    { NULL, 0 },
    { &_netlink_hdr_done, sizeof(_netlink_hdr_done) },
};

static struct msghdr _netlink_send_msg = {
  &_netlink_nladdr,
  sizeof(_netlink_nladdr),
  &_netlink_send_iov[0],
  2,
  NULL,
  0,
  0
};

/* netlink timeout handling */
static struct oonf_timer_info _netlink_timer= {
  .name = "netlink feedback timer",
  .callback = _cb_handle_netlink_timeout,
};

/* built in rtnetlink multicast receiver */
static struct os_system_netlink _rtnetlink_receiver = {
  .cb_message = _handle_rtnetlink,
};

const uint32_t _rtnetlink_mcast[] = {
  RTNLGRP_LINK, RTNLGRP_IPV4_IFADDR, RTNLGRP_IPV6_IFADDR
};

/* list of interface change listeners */
struct list_entity _ifchange_listener;

/* subsystem definition */
struct oonf_subsystem oonf_os_system_subsystem = {
  .name = "os_system",
  .init = _init,
  .cleanup = _cleanup,
};

/**
 * Initialize os-specific subsystem
 * @return -1 if an error happened, 0 otherwise
 */
static int
_init(void) {
  _ioctl_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (_ioctl_fd == -1) {
    OONF_WARN(LOG_OS_SYSTEM, "Cannot open ioctl socket: %s (%d)",
        strerror(errno), errno);
    return -1;
  }

  if (os_system_netlink_add(&_rtnetlink_receiver, NETLINK_ROUTE)) {
    close(_ioctl_fd);
    return -1;
  }

  if (os_system_netlink_add_mc(&_rtnetlink_receiver, _rtnetlink_mcast, ARRAYSIZE(_rtnetlink_mcast))) {
    os_system_netlink_remove(&_rtnetlink_receiver);
    close(_ioctl_fd);
    return -1;
  }

  oonf_timer_add(&_netlink_timer);
  list_init_head(&_ifchange_listener);
  return 0;
}

/**
 * Cleanup os-specific subsystem
 */
static void
_cleanup(void) {
  oonf_timer_remove(&_netlink_timer);
  os_system_netlink_remove(&_rtnetlink_receiver);
  close(_ioctl_fd);
}

/**
 * Set interface up or down
 * @param dev pointer to name of interface
 * @param up true if interface should be up, false if down
 * @return -1 if an error happened, 0 otherwise
 */
int
os_system_set_interface_state(const char *dev, bool up) {
  int oldflags;
  struct ifreq ifr;

  memset(&ifr, 0, sizeof(ifr));
  strscpy(ifr.ifr_name, dev, IFNAMSIZ);

  if (ioctl(_ioctl_fd, SIOCGIFFLAGS, &ifr) < 0) {
    OONF_WARN(LOG_OS_SYSTEM,
        "ioctl SIOCGIFFLAGS (get flags) error on device %s: %s (%d)\n",
        dev, strerror(errno), errno);
    return -1;
  }

  oldflags = ifr.ifr_flags;
  if (up) {
    ifr.ifr_flags |= IFF_UP;
  }
  else {
    ifr.ifr_flags &= ~IFF_UP;
  }

  if (oldflags == ifr.ifr_flags) {
    /* interface is already up/down */
    return 0;
  }

  if (ioctl(_ioctl_fd, SIOCSIFFLAGS, &ifr) < 0) {
    OONF_WARN(LOG_OS_SYSTEM,
        "ioctl SIOCSIFFLAGS (set flags %s) error on device %s: %s (%d)\n",
        up ? "up" : "down", dev, strerror(errno), errno);
    return -1;
  }
  return 0;
}

void
os_system_iflistener_add(struct os_system_if_listener *listener) {
  list_add_tail(&_ifchange_listener, &listener->_node);
}

void
os_system_iflistener_remove(struct os_system_if_listener *listener) {
  list_remove(&listener->_node);
}

/**
 * Open a new bidirectional netlink socket
 * @param nl pointer to initialized netlink socket handler
 * @param protocol protocol id (NETLINK_ROUTING for example)
 * @return -1 if an error happened, 0 otherwise
 */
int
os_system_netlink_add(struct os_system_netlink *nl, int protocol) {
  struct sockaddr_nl addr;

  nl->socket.fd = socket(PF_NETLINK, SOCK_RAW, protocol);
  if (nl->socket.fd < 0) {
    OONF_WARN(LOG_OS_SYSTEM, "Cannot open sync rtnetlink socket: %s (%d)",
        strerror(errno), errno);
    goto os_add_netlink_fail;
  }

  if (abuf_init(&nl->out)) {
    OONF_WARN(LOG_OS_SYSTEM, "Not enough memory for netlink output buffer");
    goto os_add_netlink_fail;
  }

  nl->in = calloc(1, getpagesize());
  if (nl->in == NULL) {
    OONF_WARN(LOG_OS_SYSTEM, "Not enough memory for netlink input buffer");
    goto os_add_netlink_fail;
  }
  nl->in_len = getpagesize();

  memset(&addr, 0, sizeof(addr));
  addr.nl_family = AF_NETLINK;

  /* kernel will assign appropriate number instead of pid */
  /* addr.nl_pid = 0; */

  if (bind(nl->socket.fd, (struct sockaddr *)&addr, sizeof(addr))<0) {
    OONF_WARN(LOG_OS_SYSTEM, "Could not bind netlink socket: %s (%d)",
        strerror(errno), errno);
    goto os_add_netlink_fail;
  }

  nl->socket.process = _netlink_handler;
  nl->socket.event_read = true;
  nl->socket.data = nl;
  oonf_socket_add(&nl->socket);

  nl->timeout.cb_context = nl;
  nl->timeout.info = &_netlink_timer;

  return 0;

os_add_netlink_fail:
  if (nl->socket.fd != -1) {
    close(nl->socket.fd);
  }
  free (nl->in);
  abuf_free(&nl->out);
  return -1;
}

/**
 * Close a netlink socket handler
 * @param nl pointer to handler
 */
void
os_system_netlink_remove(struct os_system_netlink *nl) {
  oonf_socket_remove(&nl->socket);

  close(nl->socket.fd);
  free (nl->in);
  abuf_free(&nl->out);
}

/**
 * Add a netlink message to the outgoign queue of a handler
 * @param nl pointer to netlink handler
 * @param nl_hdr pointer to netlink message
 * @return sequence number used for message
 */
int
os_system_netlink_send(struct os_system_netlink *nl,
    struct nlmsghdr *nl_hdr) {
  nl->seq_used = (nl->seq_used + 1) & INT32_MAX;

  nl_hdr->nlmsg_seq = nl->seq_used;
  nl_hdr->nlmsg_flags |= NLM_F_ACK | NLM_F_MULTI;

  abuf_memcpy(&nl->out, nl_hdr, nl_hdr->nlmsg_len);

  /* trigger write */
  oonf_socket_set_write(&nl->socket, true);
  return nl->seq_used;
}

/**
 * Join a list of multicast groups for a netlink socket
 * @param nl pointer to netlink handler
 * @param groups pointer to array of multicast groups
 * @param groupcount number of entries in groups array
 * @return -1 if an error happened, 0 otherwise
 */
int
os_system_netlink_add_mc(struct os_system_netlink *nl,
    const uint32_t *groups, size_t groupcount) {
  size_t i;

  for (i=0; i<groupcount; i++) {
    if (setsockopt(nl->socket.fd, SOL_NETLINK, NETLINK_ADD_MEMBERSHIP,
             &groups[i], sizeof(groups[i]))) {
      OONF_WARN(LOG_OS_SYSTEM,
          "Could not join netlink mc group: %x", groups[i]);
      return -1;
    }
  }
  return 0;
}

/**
 * Leave a list of multicast groups for a netlink socket
 * @param nl pointer to netlink handler
 * @param groups pointer to array of multicast groups
 * @param groupcount number of entries in groups array
 * @return -1 if an error happened, 0 otherwise
 */
int
os_system_netlink_drop_mc(struct os_system_netlink *nl,
    const int *groups, size_t groupcount) {
  size_t i;

  for (i=0; i<groupcount; i++) {
    if (setsockopt(nl->socket.fd, SOL_NETLINK, NETLINK_DROP_MEMBERSHIP,
             &groups[i], sizeof(groups[i]))) {
      OONF_WARN(LOG_OS_SYSTEM,
          "Could not drop netlink mc group: %x", groups[i]);
      return -1;
    }
  }
  return 0;
}

/**
 * Add an attribute to a netlink message
 * @param n pointer to netlink header
 * @param type type of netlink attribute
 * @param data pointer to data of netlink attribute
 * @param len length of data of netlink attribute
 * @return -1 if netlink message got too large, 0 otherwise
 */
int
os_system_netlink_addreq(struct nlmsghdr *n,
    int type, const void *data, int len)
{
  struct nlattr *nl_attr;
  size_t aligned_msg_len, aligned_attr_len;

  /* calculate aligned length of message and new attribute */
  aligned_msg_len = NLMSG_ALIGN(n->nlmsg_len);
  aligned_attr_len = NLA_HDRLEN + len;

  if (aligned_msg_len + aligned_attr_len > UIO_MAXIOV) {
    OONF_WARN(LOG_OS_SYSTEM, "Netlink message got too large!");
    return -1;
  }

  nl_attr = (struct nlattr *) ((void*)((char *)n + aligned_msg_len));
  nl_attr->nla_type = type;
  nl_attr->nla_len = aligned_attr_len;

  /* fix length of netlink message */
  n->nlmsg_len = aligned_msg_len + aligned_attr_len;

  memcpy((char *)nl_attr + NLA_HDRLEN, data, len);
  return 0;
}

/**
 * Handle timeout of netlink acks
 * @param ptr pointer to netlink handler
 */
static void
_cb_handle_netlink_timeout(void *ptr) {
  struct os_system_netlink *nl = ptr;

  if (nl->cb_timeout) {
    nl->cb_timeout();
  }

  nl->seq_used = 0;
}

/**
 * Send all netlink messages in the outgoing queue to the kernel
 * @param nl pointer to netlink handler
 */
static void
_flush_netlink_buffer(struct os_system_netlink *nl) {
  ssize_t ret;

  /* start feedback timer */
  oonf_timer_set(&nl->timeout, OS_SYSTEM_NETLINK_TIMEOUT);

  /* send outgoing message */
  _netlink_send_iov[0].iov_base = abuf_getptr(&nl->out);
  _netlink_send_iov[0].iov_len = abuf_getlen(&nl->out);

  if ((ret = sendmsg(nl->socket.fd, &_netlink_send_msg, 0)) <= 0) {
    OONF_WARN(LOG_OS_SYSTEM,
        "Cannot send data to netlink socket (%d: %s)",
        errno, strerror(errno));
  }
  else {
    OONF_DEBUG(LOG_OS_SYSTEM, "Sent %zd/%zu bytes for netlink seqno: %d",
        ret, abuf_getlen(&nl->out), nl->seq_used);
    nl->seq_sent = nl->seq_used;
    abuf_clear(&nl->out);

    oonf_socket_set_write(&nl->socket, false);
  }
}

/**
 * Cleanup netlink handler because all outstanding jobs
 * are finished
 * @param nl pointer to os_system_netlink handler
 */
static void
_netlink_job_finished(struct os_system_netlink *nl) {
  if (nl->msg_in_transit > 0) {
    nl->msg_in_transit--;
  }
  if (nl->msg_in_transit == 0) {
    oonf_timer_stop(&nl->timeout);
    nl->seq_used = 0;
  }
}

/**
 * Handler for incoming netlink messages
 * @param fd
 * @param data
 * @param event_read
 * @param event_write
 */
static void
_netlink_handler(int fd, void *data, bool event_read, bool event_write) {
  struct os_system_netlink *nl;
  struct nlmsghdr *nh;
  ssize_t ret;
  size_t len;
  int flags;

  nl = data;
  if (event_write) {
    _flush_netlink_buffer(nl);
  }

  if (!event_read) {
    return;
  }

  /* handle incoming messages */
  _netlink_rcv_msg.msg_flags = 0;
  flags = MSG_PEEK;

netlink_rcv_retry:
  _netlink_rcv_iov.iov_base = nl->in;
  _netlink_rcv_iov.iov_len = nl->in_len;

  if ((ret = recvmsg(fd, &_netlink_rcv_msg, MSG_DONTWAIT | flags)) < 0) {
    if (errno != EAGAIN) {
      OONF_WARN(LOG_OS_SYSTEM,"netlink recvmsg error: %s (%d)\n",
          strerror(errno), errno);
    }
    return;
  }

  /* not enough buffer space ? */
  if (nl->in_len < (size_t)ret || (_netlink_rcv_msg.msg_flags & MSG_TRUNC) != 0) {
    void *ptr;
    size_t size;

    size = nl->in_len;
    while (size < (size_t)ret) {
      size += getpagesize();
    }
    ptr = realloc(nl->in, size);
    if (!ptr) {
      OONF_WARN(LOG_OS_SYSTEM, "Not enough memory to increase netlink input buffer");
      return;
    }
    nl->in = ptr;
    nl->in_len = size;
    goto netlink_rcv_retry;
  }
  if (flags) {
    /* it worked, not remove the message from the queue */
    flags = 0;
    goto netlink_rcv_retry;
  }

  OONF_DEBUG(LOG_OS_SYSTEM, "Got netlink message of %"
      PRINTF_SSIZE_T_SPECIFIER" bytes", ret);

  /* loop through netlink headers */
  len = (size_t) ret;
  for (nh = nl->in; NLMSG_OK (nh, len); nh = NLMSG_NEXT (nh, len)) {
    OONF_DEBUG(LOG_OS_SYSTEM,
        "Netlink message received: type %d\n", nh->nlmsg_type);

    switch (nh->nlmsg_type) {
      case NLMSG_NOOP:
        break;

      case NLMSG_DONE:
        OONF_DEBUG(LOG_OS_SYSTEM, "Netlink message done: %d", nh->nlmsg_seq);
        if (nl->cb_done) {
          nl->cb_done(nh->nlmsg_seq);
        }
        _netlink_job_finished(nl);
        /* End of a multipart netlink message reached */
        break;

      case NLMSG_ERROR:
        /* Feedback for async netlink message */
        _handle_nl_err(nl, nh);
        break;

      default:
        if (nl->cb_message) {
          nl->cb_message(nh);
        }
        break;
    }
  }
}

/**
 * Handle incoming rtnetlink multicast messages for interface listeners
 * @param hdr pointer to netlink message
 */
static void
_handle_rtnetlink(struct nlmsghdr *hdr) {
  struct ifinfomsg *ifi;
  struct ifaddrmsg *ifa;

  struct os_system_if_listener *listener;
  char if_name[IF_NAMESIZE];

  if (hdr->nlmsg_type == RTM_NEWLINK || hdr->nlmsg_type == RTM_DELLINK) {
    ifi = (struct ifinfomsg *) NLMSG_DATA(hdr);

    if (if_indextoname(ifi->ifi_index, if_name) == NULL) {
      OONF_WARN(LOG_OS_SYSTEM,
          "Failed to convert if-index to name: %d", ifi->ifi_index);
      return;
    }

    OONF_DEBUG(LOG_OS_SYSTEM, "Linkstatus of interface '%s' changed", if_name);
    list_for_each_element(&_ifchange_listener, listener, _node) {
      listener->if_changed(if_name, (ifi->ifi_flags & IFF_UP) == 0);
    }
  }

  else if (hdr->nlmsg_type == RTM_NEWADDR || hdr->nlmsg_type == RTM_DELADDR) {
    ifa = (struct ifaddrmsg *) NLMSG_DATA(hdr);

    if (if_indextoname(ifa->ifa_index, if_name) == NULL) {
      OONF_WARN(LOG_OS_SYSTEM,
          "Failed to convert if-index to name: %d", ifa->ifa_index);
      return;
    }

    OONF_DEBUG(LOG_OS_SYSTEM, "Address of interface '%s' changed", if_name);
    list_for_each_element(&_ifchange_listener, listener, _node) {
      listener->if_changed(if_name, false);
    }
  }
}

/**
 * Handle result code in netlink message
 * @param nl pointer to netlink handler
 * @param nh pointer to netlink message
 */
static void
_handle_nl_err(struct os_system_netlink *nl, struct nlmsghdr *nh) {
  struct nlmsgerr *err;

  err = (struct nlmsgerr *) NLMSG_DATA(nh);

  OONF_DEBUG(LOG_OS_SYSTEM, "Received netlink feedback (%u bytes): %s (%d)",
      nh->nlmsg_len, strerror(-err->error), err->error);

  if (nl->cb_error) {
    nl->cb_error(err->msg.nlmsg_seq, err->error);
  }
  _netlink_job_finished(nl);
}

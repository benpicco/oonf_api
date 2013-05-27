
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

#include <errno.h>
#include <fcntl.h>

#include "common/common_types.h"
#include "common/list.h"
#include "common/autobuf.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/os_net.h"
#include "subsystems/oonf_packet_socket.h"
#include "subsystems/oonf_interface.h"
#include "subsystems/oonf_socket.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static int _apply_managed(struct oonf_packet_managed *managed);
static int _get_socket_bindaddress(struct netaddr *bindto, int af_type,
    struct netaddr_acl *filter, struct oonf_interface_data *ifdata);
static int _apply_managed_socketpair(int af_type,
    struct oonf_packet_managed *managed,
    struct oonf_interface_data *data, bool *changed,
    struct oonf_packet_socket *sock, struct netaddr_acl *bind_ip,
    struct oonf_packet_socket *mc_sock, struct netaddr *mc_ip);
static int _apply_managed_socket(struct oonf_packet_managed *managed,
    struct oonf_packet_socket *stream, struct netaddr *bindto, int port,
    struct oonf_interface_data *data);
static void _cb_packet_event_unicast(int fd, void *data, bool r, bool w);
static void _cb_packet_event_multicast(int fd, void *data, bool r, bool w);
static void _cb_packet_event(int fd, void *data, bool r, bool w, bool mc);
static void _cb_interface_listener(struct oonf_interface_listener *l);

/* subsystem definition */
struct oonf_subsystem oonf_packet_socket_subsystem = {
  .name = "packet",
  .init = _init,
  .cleanup = _cleanup,
};

/* other global variables */
static struct list_entity packet_sockets = { NULL, NULL };
static char input_buffer[65536];

/**
 * Initialize packet socket handler
 * @return always returns 0
 */
static int
_init(void) {
  list_init_head(&packet_sockets);
  return 0;
}

/**
 * Cleanup all resources allocated by packet socket handler
 */
static void
_cleanup(void) {
  struct oonf_packet_socket *skt;

  while (!list_is_empty(&packet_sockets)) {
    skt = list_first_element(&packet_sockets, skt, node);

    oonf_packet_remove(skt, true);
  }
}

/**
 * Add a new packet socket handler
 * @param pktsocket pointer to an initialized packet socket struct
 * @param local pointer local IP address of packet socket
 * @param interf pointer to interface to bind socket on, NULL
 *   if socket should not be bound to interface
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_packet_add(struct oonf_packet_socket *pktsocket,
    union netaddr_socket *local, struct oonf_interface_data *interf) {
  int s = -1;

  /* Init socket */
  s = os_net_getsocket(local, false, 0, interf, LOG_PACKET);
  if (s < 0) {
    return -1;
  }

  pktsocket->interface = interf;
  pktsocket->scheduler_entry.fd = s;
  pktsocket->scheduler_entry.process = _cb_packet_event_unicast;
  pktsocket->scheduler_entry.event_read = true;
  pktsocket->scheduler_entry.event_write = false;
  pktsocket->scheduler_entry.data = pktsocket;

  oonf_socket_add(&pktsocket->scheduler_entry);

  abuf_init(&pktsocket->out);
  list_add_tail(&packet_sockets, &pktsocket->node);
  memcpy(&pktsocket->local_socket, local, sizeof(pktsocket->local_socket));

  if (pktsocket->config.input_buffer_length == 0) {
    pktsocket->config.input_buffer = input_buffer;
    pktsocket->config.input_buffer_length = sizeof(input_buffer);
  }
  return 0;
}

/**
 * Remove a packet socket from the global scheduler
 * @param pktsocket pointer to packet socket
 * @param force true if the socket should be removed instantly,
 *   false if it should be removed after the last packet in queue is sent
 */
void
oonf_packet_remove(struct oonf_packet_socket *pktsocket,
    bool force __attribute__((unused))) {
  // TODO: implement non-force behavior for UDP sockets
  if (list_is_node_added(&pktsocket->node)) {
    oonf_socket_remove(&pktsocket->scheduler_entry);
    os_close(pktsocket->scheduler_entry.fd);
    abuf_free(&pktsocket->out);

    list_remove(&pktsocket->node);

    pktsocket->scheduler_entry.fd = -1;
  }
}

/**
 * Send a data packet through a packet socket. The transmission might not
 * be happen synchronously if the socket would block.
 * @param pktsocket pointer to packet socket
 * @param remote ip/address to send packet to
 * @param data pointer to data to be sent
 * @param length length of data
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_packet_send(struct oonf_packet_socket *pktsocket, union netaddr_socket *remote,
    const void *data, size_t length) {
  int result;
  struct netaddr_str buf;

  if (abuf_getlen(&pktsocket->out) == 0) {
    /* no backlog of outgoing packets, try to send directly */
    result = os_sendto(pktsocket->scheduler_entry.fd, data, length, remote);
    if (result > 0) {
      /* successful */
      OONF_DEBUG(LOG_PACKET, "Sent %d bytes to %s %s",
          result, netaddr_socket_to_string(&buf, remote),
          pktsocket->interface != NULL ? pktsocket->interface->name : "");
      return 0;
    }

    if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) {
      OONF_WARN(LOG_PACKET, "Cannot send UDP packet to %s: %s (%d)",
          netaddr_socket_to_string(&buf, remote), strerror(errno), errno);
      return -1;
    }
  }

  /* append destination */
  abuf_memcpy(&pktsocket->out, remote, sizeof(*remote));

  /* append data length */
  abuf_append_uint16(&pktsocket->out, length);

  /* append data */
  abuf_memcpy(&pktsocket->out, data, length);

  /* activate outgoing socket scheduler */
  oonf_socket_set_write(&pktsocket->scheduler_entry, true);
  return 0;
}

/**
 * Initialize a new managed packet socket
 * @param managed pointer to packet socket
 */
void
oonf_packet_add_managed(struct oonf_packet_managed *managed) {
  if (managed->config.input_buffer_length == 0) {
    managed->config.input_buffer = input_buffer;
    managed->config.input_buffer_length = sizeof(input_buffer);
  }

  managed->_if_listener.process = _cb_interface_listener;
  managed->_if_listener.name = managed->_managed_config.interface;
  managed->_if_listener.mesh = managed->_managed_config.mesh;
}

/**
 * Cleanup an initialized managed packet socket
 * @param managed pointer to packet socket
 * @param forced true if socket should be closed instantly
 */
void
oonf_packet_remove_managed(struct oonf_packet_managed *managed, bool forced) {
  oonf_packet_remove(&managed->socket_v4, forced);
  oonf_packet_remove(&managed->socket_v6, forced);
  oonf_packet_remove(&managed->multicast_v4, forced);
  oonf_packet_remove(&managed->multicast_v6, forced);

  oonf_interface_remove_listener(&managed->_if_listener);
  netaddr_acl_remove(&managed->_managed_config.acl);
}

/**
 * Apply a new configuration to a managed socket. This might close and
 * reopen sockets because of changed binding IPs or ports.
 * @param managed pointer to managed packet socket
 * @param config pointer to new configuration
 * @return -1 if an error happened, 0 otherwise
 */
int
oonf_packet_apply_managed(struct oonf_packet_managed *managed,
    struct oonf_packet_managed_config *config) {
  bool if_changed;

  if_changed = strcmp(config->interface, managed->_managed_config.interface) != 0;

  /* copy config */
  memcpy(&managed->_managed_config, config, sizeof(*config));
  managed->_if_listener.mesh = config->mesh;

  /* copy acl */
  memset(&managed->_managed_config.acl, 0, sizeof(managed->_managed_config.acl));
  netaddr_acl_copy(&managed->_managed_config.acl, &config->acl);

  /* handle change in interface listener */
  if (if_changed) {
    /* interface changed, remove old listener if necessary */
    oonf_interface_remove_listener(&managed->_if_listener);

    if (managed->_managed_config.interface[0]) {
      /* create new interface listener */
      oonf_interface_add_listener(&managed->_if_listener);
    }
  }

  OONF_DEBUG(LOG_PACKET, "Apply changes for managed socket (if %s) with port %d/%d",
      config->interface == NULL || config->interface[0] == 0 ? "any" : config->interface,
      config->port, config->multicast_port);
  return _apply_managed(managed);
}

/**
 * Send a packet out over one of the managed sockets, depending on the
 * address family type of the remote address
 * @param managed pointer to managed packet socket
 * @param remote pointer to remote socket
 * @param data pointer to data to send
 * @param length length of data
 * @return -1 if an error happened, 0 if packet was sent, 1 if this
 *    type of address was switched off
 */
int
oonf_packet_send_managed(struct oonf_packet_managed *managed,
    union netaddr_socket *remote, const void *data, size_t length) {
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf;
#endif

  if (netaddr_socket_get_addressfamily(remote) == AF_UNSPEC) {
    return 0;
  }

  if (list_is_node_added(&managed->socket_v4.scheduler_entry._node)
      && netaddr_socket_get_addressfamily(remote) == AF_INET) {
    return oonf_packet_send(&managed->socket_v4, remote, data, length);
  }
  if (list_is_node_added(&managed->socket_v6.scheduler_entry._node)
      && netaddr_socket_get_addressfamily(remote) == AF_INET6) {
    return oonf_packet_send(&managed->socket_v6, remote, data, length);
  }
  errno = 0;
  OONF_DEBUG(LOG_PACKET,
      "Managed socket did not sent packet to %s because socket was not active",
      netaddr_socket_to_string(&buf, remote));

  return 0;
}

/**
 * Send a packet out over one of the managed sockets, depending on the
 * address family type of the remote address
 * @param managed pointer to managed packet socket
 * @param data pointer to data to send
 * @param length length of data
 * @param af_type address family to send multicast
 * @return -1 if an error happened, 0 if packet was sent, 1 if this
 *    type of address was switched off
 */
int
oonf_packet_send_managed_multicast(struct oonf_packet_managed *managed,
    const void *data, size_t length, int af_type) {
  if (af_type == AF_INET) {
    return oonf_packet_send_managed(managed, &managed->multicast_v4.local_socket, data, length);
  }
  else if (af_type == AF_INET6) {
    return oonf_packet_send_managed(managed, &managed->multicast_v6.local_socket, data, length);
  }
  errno = 0;
  return 1;
}

/**
 * Returns true if the socket for IPv4/6 is active to send data.
 * @param managed pointer to managed UDP socket
 * @param af_type address familty
 * @return true if the selected socket is active.
 */
bool
oonf_packet_managed_is_active(
    struct oonf_packet_managed *managed, int af_type) {
  switch (af_type) {
    case AF_INET:
      return oonf_packet_is_active(&managed->socket_v4);
    case AF_INET6:
      return oonf_packet_is_active(&managed->socket_v6);
    default:
      return false;
  }
}

/**
 * Apply a new configuration to all attached sockets
 * @param managed pointer to managed socket
 * @param config pointer to configuration
 * @return -1 if an error happened, 0 otherwise
 */
static int
_apply_managed(struct oonf_packet_managed *managed) {
  struct oonf_interface_data *data = NULL;
  bool changed = false;
  int result = 0;

  /* get interface */
  if (managed->_if_listener.interface) {
    data = &managed->_if_listener.interface->data;
  }

  if (_apply_managed_socketpair(AF_INET, managed, data, &changed,
      &managed->socket_v4, &managed->_managed_config.bindto,
      &managed->multicast_v4, &managed->_managed_config.multicast_v4)) {
    result = -1;
  }

  if (_apply_managed_socketpair(AF_INET6, managed, data, &changed,
      &managed->socket_v6, &managed->_managed_config.bindto,
      &managed->multicast_v6, &managed->_managed_config.multicast_v6)) {
    result = -1;
  }

  if (managed->cb_settings_change) {
    managed->cb_settings_change(managed, changed);
  }
  return result;
}

/**
 * Calculate the IP address a socket should bind to
 * @param bindto pointer to buffer for result, will only be
 *   overwritten if a fitting address is found.
 * @param filter filter for IP address to bind on
 * @param ifdata interface to bind to socket on, NULL if not
 *   bound to an interface.
 * @return 0 if an IP was calculated, -1 otherwise
 */
static int
_get_socket_bindaddress(struct netaddr *bindto, int af_type,
    struct netaddr_acl *filter, struct oonf_interface_data *ifdata) {
  size_t i;

  if (ifdata == NULL) {
    /*
     * run through accept list of filter, looking for address
     * with full prefix length or zero prefix length
     */
    for (i=0; i<filter->accept_count; i++) {
      if (netaddr_get_address_family(&filter->accept[i]) != af_type) {
        continue;
      }

      if (netaddr_get_prefix_length(&filter->accept[i]) == 0
          || netaddr_get_prefix_length(&filter->accept[i])
             == netaddr_get_maxprefix(&filter->accept[i])) {
        /* full address or any address match */
        memcpy(bindto, &filter->accept[i], sizeof(*bindto));
        return 0;
      }
    }
  }
  else {
    /* run through interface address list looking for filter match */
    for (i=0; i<ifdata->addrcount; i++) {
      if (netaddr_get_address_family(&ifdata->addresses[i]) != af_type) {
        continue;
      }

      if (netaddr_acl_check_accept(filter, &ifdata->addresses[i])) {
        memcpy(bindto, &ifdata->addresses[i], sizeof(*bindto));
        return 0;
      }
    }
  }
  return -1;
}

/**
 * Apply a new configuration to an unicast/multicast socket pair
 * @param managed pointer to managed socket
 * @param data pointer to interface to bind sockets, NULL if unbound socket
 * @param sock pointer to unicast packet socket
 * @param bind_ip address to bind unicast socket to
 * @param mc_sock pointer to multicast packet socket
 * @param mc_ip multicast address
 * @return
 */
static int
_apply_managed_socketpair(int af_type, struct oonf_packet_managed *managed,
    struct oonf_interface_data *data, bool *changed,
    struct oonf_packet_socket *sock, struct netaddr_acl *bind_ip_acl,
    struct oonf_packet_socket *mc_sock, struct netaddr *mc_ip) {
  int sockstate = 0, result = 0;
  uint16_t mc_port;
  bool real_multicast;
  struct netaddr bind_ip;

  /* copy unicast port if necessary */
  mc_port = managed->_managed_config.multicast_port;
  if (mc_port == 0) {
    mc_port = managed->_managed_config.port;
  }

  /* Get address the unicast socket should bind on */
  if (_get_socket_bindaddress(&bind_ip, af_type, bind_ip_acl, data)) {
    oonf_packet_remove(sock, false);
    oonf_packet_remove(mc_sock, false);
    return 0;
  }

  /* handle loopback interface */
  if (data != NULL && data->loopback
      && netaddr_get_address_family(mc_ip) != AF_UNSPEC) {
    memcpy(mc_ip, &bind_ip, sizeof(*mc_ip));
  }

  /* check if multicast IP is a real multicast (and not a broadcast) */
  real_multicast = netaddr_is_in_subnet(
      netaddr_get_address_family(mc_ip) == AF_INET
        ? &NETADDR_IPV4_MULTICAST : &NETADDR_IPV6_MULTICAST,
      mc_ip);

  sockstate = _apply_managed_socket(
      managed, sock, &bind_ip, managed->_managed_config.port, data);
  if (sockstate == 0) {
    /* settings really changed */
    *changed = true;

    if (real_multicast && data != NULL && data->up) {
      os_net_join_mcast_send(sock->scheduler_entry.fd,
          mc_ip, data, managed->_managed_config.loop_multicast, LOG_PACKET);
    }
  }
  else if (sockstate < 0) {
    /* error */
    result = -1;
    oonf_packet_remove(sock, true);
  }

  if (real_multicast && netaddr_get_address_family(mc_ip) != AF_UNSPEC) {
    /* multicast */
    sockstate = _apply_managed_socket(
        managed, mc_sock, mc_ip, mc_port, data);
    if (sockstate == 0) {
      /* settings really changed */
      *changed = true;

      mc_sock->scheduler_entry.process = _cb_packet_event_multicast;

      /* join multicast group */
      os_net_join_mcast_recv(mc_sock->scheduler_entry.fd,
          mc_ip, data, LOG_PACKET);
    }
    else if (sockstate < 0) {
      /* error */
      result = -1;
      oonf_packet_remove(sock, true);
    }
  }
  else {
    oonf_packet_remove(mc_sock, true);

    /*
     * initialize anyways because we use it for sending broadcasts with
     * oonf_packet_send_managed_multicast()
     */
    netaddr_socket_init(&mc_sock->local_socket, mc_ip, mc_port,
        data == NULL ? 0 : data->index);
  }
  return result;
}

/**
 * Apply new configuration to a managed stream socket
 * @param managed pointer to managed stream
 * @param stream pointer to TCP stream to configure
 * @param bindto local address to bind socket to
 *   set to AF_UNSPEC for simple reinitialization
 * @param port local port number
 * @param if_event true if this is just a reapply of current values
 *   because interface came up again.
 * @return -1 if an error happened, 0 if everything is okay,
 *   1 if the socket wasn't touched.
 */
static int
_apply_managed_socket(struct oonf_packet_managed *managed,
    struct oonf_packet_socket *packet,
    struct netaddr *bindto, int port,
    struct oonf_interface_data *data) {
  union netaddr_socket sock;
  struct netaddr_str buf;

  /* create binding socket */
  if (netaddr_socket_init(&sock, bindto, port,
      data == NULL ? 0 : data->index)) {
    OONF_WARN(LOG_PACKET, "Cannot create managed socket address: %s/%u",
        netaddr_to_string(&buf, bindto), port);
    return -1;
  }

  if (list_is_node_added(&packet->node)) {
    if (data == packet->interface
        && memcmp(&sock, &packet->local_socket, sizeof(sock)) == 0) {
      /* nothing changed */
      return 1;
    }
  }
  else {
    if (data != NULL && !data->up) {
      /* nothing changed */
      return 1;
    }
  }

  /* remove old socket */
  oonf_packet_remove(packet, true);

  if (data != NULL && !data->up) {
    OONF_DEBUG(LOG_PACKET, "Interface %s of socket is down",
        data->name);
    return 0;
  }

  /* copy configuration */
  memcpy(&packet->config, &managed->config, sizeof(packet->config));
  if (packet->config.user == NULL) {
    packet->config.user = managed;
  }

  /* create new socket */
  if (oonf_packet_add(packet, &sock, data)) {
    return -1;
  }

  packet->interface = data;

  OONF_DEBUG(LOG_PACKET, "Opened new socket and bound it to %s (if %s)",
      netaddr_to_string(&buf, bindto),
      data != NULL ? data->name : "any");
  return 0;
}

/**
 * callback for unicast events in socket scheduler
 * @param fd
 * @param data
 * @param event_read
 * @param event_write
 */
static void
_cb_packet_event_unicast(int fd, void *data, bool event_read, bool event_write) {
  _cb_packet_event(fd, data, event_read, event_write, false);
}

/**
 * callback for multicast events in socket scheduler
 * @param fd
 * @param data
 * @param event_read
 * @param event_write
 */
static void
_cb_packet_event_multicast(int fd, void *data, bool event_read, bool event_write) {
  _cb_packet_event(fd, data, event_read, event_write, true);
}

/**
 * Callback to handle data from the olsr socket scheduler
 * @param fd filedescriptor to read data from
 * @param data custom data pointer
 * @param event_read true if read-event is incoming
 * @param event_write true if write-event is incoming
 */
static void
_cb_packet_event(int fd, void *data, bool event_read, bool event_write,
    bool multicast __attribute__((unused))) {
  struct oonf_packet_socket *pktsocket = data;
  union netaddr_socket *skt, sock;
  uint16_t length;
  char *pkt;
  int result;
  struct netaddr_str netbuf;

#ifdef OONF_LOG_DEBUG_INFO
  const char *interf = "";

  if (pktsocket->interface) {
    interf = pktsocket->interface->name;
  }
#endif

  if (event_read) {
    uint8_t *buf;

    /* clear recvfrom memory */
    memset(&sock, 0, sizeof(sock));

    /* handle incoming data */
    buf = pktsocket->config.input_buffer;

    result = os_recvfrom(fd, buf, pktsocket->config.input_buffer_length-1, &sock,
        pktsocket->interface);
    if (result > 0 && pktsocket->config.receive_data != NULL) {
      /* null terminate it */
      buf[result] = 0;

      /* received valid packet */
      OONF_DEBUG(LOG_PACKET, "Received %d bytes from %s %s (%s)",
          result, netaddr_socket_to_string(&netbuf, &sock),
          interf, multicast ? "multicast" : "unicast");
      pktsocket->config.receive_data(pktsocket, &sock, result);
    }
    else if (result < 0 && (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK)) {
      OONF_WARN(LOG_PACKET, "Cannot read packet from socket %s: %s (%d)",
          netaddr_socket_to_string(&netbuf, &pktsocket->local_socket), strerror(errno), errno);
    }
  }

  if (event_write && abuf_getlen(&pktsocket->out) > 0) {
    /* handle outgoing data */

    /* pointer to remote socket */
    skt = data;

    /* data area */
    pkt = data;
    pkt += sizeof(*skt);

    memcpy(&length, pkt, 2);
    pkt += 2;

    /* try to send packet */
    result = os_sendto(fd, data, length, skt);
    if (result < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) {
      /* try again later */
      OONF_DEBUG(LOG_PACKET, "Sending to %s %s could block, try again later",
          netaddr_socket_to_string(&netbuf, skt), interf);
      return;
    }

    if (result < 0) {
      /* display error message */
      OONF_WARN(LOG_PACKET, "Cannot send UDP packet to %s: %s (%d)",
          netaddr_socket_to_string(&netbuf, skt), strerror(errno), errno);
    }
    else {
      OONF_DEBUG(LOG_PACKET, "Sent %d bytes to %s %s",
          result, netaddr_socket_to_string(&netbuf, skt), interf);
    }
    /* remove data from outgoing buffer (both for success and for final error */
    abuf_pull(&pktsocket->out, sizeof(*skt) + 2 + length);
  }

  if (abuf_getlen(&pktsocket->out) == 0) {
    /* nothing left to send, disable outgoing events */
    oonf_socket_set_write(&pktsocket->scheduler_entry, false);
  }
}

/**
 * Callbacks for events on the interface
 * @param l
 * @param old
 */
static void
_cb_interface_listener(struct oonf_interface_listener *l) {
  struct oonf_packet_managed *managed;
#ifdef OONF_LOG_DEBUG_INFO
  int result;
#endif

  /* calculate managed socket for this event */
  managed = container_of(l, struct oonf_packet_managed, _if_listener);

#ifdef OONF_LOG_DEBUG_INFO
  result =
#endif
      _apply_managed(managed);

  OONF_DEBUG(LOG_PACKET,
      "Result from interface triggered socket reconfiguration: %d", result);
}


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

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "config/cfg_db.h"
#include "config/cfg_schema.h"
#include "rfc5444/rfc5444_iana.h"
#include "rfc5444/rfc5444_reader.h"
#include "rfc5444/rfc5444_writer.h"
#include "core/olsr_logging.h"
#include "core/olsr_memcookie.h"
#include "core/olsr_subsystem.h"
#include "core/olsr_timer.h"
#include "tools/olsr_cfg.h"
#include "tools/olsr_rfc5444.h"

/* constants and definitions */
#define MAX_PACKET_SIZE (1500-20-8)
#define MAX_MESSAGE_SIZE (1280-40-8-3)
#define ADDRTLV_BUFFER (8192)

#define _LOG_RFC5444_NAME "rfc5444"
#define _CFG_SECTION "interface"

struct _interface_config {
  struct olsr_packet_managed_config socket;

  uint64_t aggregation_interval;
};

/* prototypes */
static struct olsr_rfc5444_target *_create_target(const char *name);
static void _remove_target(struct olsr_rfc5444_target *target);

static void _cb_receive_data(struct olsr_packet_socket *,
      union netaddr_socket *from, size_t length);
static void _cb_send_packet_v4(
    struct rfc5444_writer *, struct rfc5444_writer_interface *, void *, size_t);
static void _cb_send_packet_v6(
    struct rfc5444_writer *, struct rfc5444_writer_interface *, void *, size_t);
static void _cb_forward_message(struct rfc5444_reader_tlvblock_context *context,
    uint8_t *buffer, size_t length);

static bool _cb_writer_ifselector(struct rfc5444_writer *, struct rfc5444_writer_interface *, void *);
static bool _cb_forward_ifselector(struct rfc5444_writer *, struct rfc5444_writer_interface *, void *);

static struct rfc5444_reader_addrblock_entry *_alloc_addrblock_entry(void);
static struct rfc5444_reader_tlvblock_entry *_alloc_tlvblock_entry(void);
static struct rfc5444_writer_address *_alloc_address_entry(void);
static struct rfc5444_writer_addrtlv *_alloc_addrtlv_entry(void);
static void _free_addrblock_entry(void *);
static void _free_tlvblock_entry(void *);
static void _free_address_entry(void *);
static void _free_addrtlv_entry(void *);

static void _cb_add_seqno(struct rfc5444_writer *, struct rfc5444_writer_interface *);
static void _cb_aggregation_event (void *);
static void _cb_config_changed(void);

/* memory block for rfc5444 targets plus MTU sized packet buffer */
static struct olsr_memcookie_info _target_memcookie = {
  .name = "RFC5444 Target",
  .size = sizeof(struct olsr_rfc5444_target) + 2*MAX_PACKET_SIZE,
};

static struct olsr_memcookie_info _tlvblock_memcookie = {
  .name = "RFC5444 TLVblock",
  .size = sizeof(struct rfc5444_reader_tlvblock_entry),
  .min_free_count = 32,
};

static struct olsr_memcookie_info _addrblock_memcookie = {
  .name = "RFC5444 Addrblock",
  .size = sizeof(struct rfc5444_reader_addrblock_entry),
  .min_free_count = 32,
};

static struct olsr_memcookie_info _address_memcookie = {
  .name = "RFC5444 Address",
  .size = sizeof(struct rfc5444_writer_address),
  .min_free_count = 32,
};

static struct olsr_memcookie_info _addrtlv_memcookie = {
  .name = "RFC5444 AddrTLV",
  .size = sizeof(struct rfc5444_writer_addrtlv),
  .min_free_count = 32,
};

/* timer for aggregating multiple rfc5444 messages to the same target */
static struct olsr_timer_info _aggregation_timer = {
  .name = "RFC5444 aggregation",
  .callback = _cb_aggregation_event,
};

/* configuration settings for handler */
static struct cfg_schema_section _interface_section = {
  .type = _CFG_SECTION,
  .mode = CFG_SSMODE_NAMED,
  .cb_delta_handler = _cb_config_changed,
};

static struct cfg_schema_entry _interface_entries[] = {
  CFG_MAP_ACL_V46(_interface_config, socket.acl, "acl", "default_accept",
    "Access control list for RFC5444 interface"),
  CFG_MAP_NETADDR_V4(_interface_config, socket.bindto_v4, "bindto_v4", "0.0.0.0",
    "Bind RFC5444 ipv4 socket to this address", true, true),
  CFG_MAP_NETADDR_V6(_interface_config, socket.bindto_v6, "bindto_v6", "linklocal6",
    "Bind RFC5444 ipv6 socket to this address", true, true),
  CFG_MAP_NETADDR_V4(_interface_config, socket.multicast_v4, "multicast_v4", RFC5444_MANET_MULTICAST_V4_TXT,
    "ipv4 multicast address of this socket", false, true),
  CFG_MAP_NETADDR_V6(_interface_config, socket.multicast_v6, "multicast_v6", RFC5444_MANET_MULTICAST_V6_TXT,
    "ipv6 multicast address of this socket", false, true),
  CFG_MAP_INT_MINMAX(_interface_config, socket.port, "port", RFC5444_MANET_UDP_PORT_TXT,
    "Multicast Network port for dlep interface", 1, 65535),

  CFG_MAP_CLOCK(_interface_config, aggregation_interval, "agregation_interval", "0.100",
    "Interval in seconds for message aggregation"),
};

static uint64_t _aggregation_interval;

/* tree of active rfc5444 targets */
static struct avl_tree _targets_tree;

/* rfc5444 handling */
uint8_t _msg_buffer[MAX_MESSAGE_SIZE];
uint8_t _addrtlv_buffer[ADDRTLV_BUFFER];

static struct rfc5444_reader _reader = {
  .forward_message = _cb_forward_message,
  .malloc_addrblock_entry = _alloc_addrblock_entry,
  .malloc_tlvblock_entry = _alloc_tlvblock_entry,
  .free_addrblock_entry = _free_addrblock_entry,
  .free_tlvblock_entry = _free_tlvblock_entry,
};
static struct rfc5444_writer _writer = {
  .msg_buffer = _msg_buffer,
  .msg_size = sizeof(_msg_buffer),
  .addrtlv_buffer = _addrtlv_buffer,
  .addrtlv_size = sizeof(_addrtlv_buffer),
  .malloc_address_entry = _alloc_address_entry,
  .malloc_addrtlv_entry = _alloc_addrtlv_entry,
  .free_address_entry = _free_address_entry,
  .free_addrtlv_entry = _free_addrtlv_entry,
};

/* configuration for RFC5444 socket */
uint8_t _incoming_buffer[MAX_PACKET_SIZE];

struct olsr_packet_config _socket_config = {
  .input_buffer = _incoming_buffer,
  .input_buffer_length = sizeof(_incoming_buffer),
  .receive_data = _cb_receive_data,
};

/* session data for an ongoing rfc5444 parsing */
union netaddr_socket *_current_source;

/* session data for an ongoing rfc5444 writer */
bool _send_ipv4, _send_ipv6;

/* rfc5444 handler state and logging source */
OLSR_SUBSYSTEM_STATE(_rfc5444_state);
static enum log_source LOG_RFC5444;

/**
 * Initialize RFC5444 handling system
 * @return -1 if an error happened, 0 otherwise
 */
void
olsr_rfc5444_init(void) {
  if (olsr_subsystem_init(&_rfc5444_state))
    return;

  LOG_RFC5444 = olsr_log_register_source(_LOG_RFC5444_NAME);

  olsr_memcookie_add(&_target_memcookie);
  olsr_memcookie_add(&_addrblock_memcookie);
  olsr_memcookie_add(&_tlvblock_memcookie);
  olsr_memcookie_add(&_address_memcookie);
  olsr_memcookie_add(&_addrtlv_memcookie);

  avl_init(&_targets_tree, avl_comp_strcasecmp, false, NULL);

  rfc5444_reader_init(&_reader);
  rfc5444_writer_init(&_writer);

  olsr_timer_add(&_aggregation_timer);

  cfg_schema_add_section(olsr_cfg_get_schema(), &_interface_section,
      _interface_entries, ARRAYSIZE(_interface_entries));

  _current_source = NULL;
}

/**
 * Cleanup all allocated resources of RFC5444 handling
 */
void
olsr_rfc5444_cleanup(void) {
  struct olsr_rfc5444_target *target, *t_it;
  if (olsr_subsystem_cleanup(&_rfc5444_state))
    return;

  /* cleanup existing interfaces */
  avl_for_each_element_safe(&_targets_tree, target, _node, t_it) {
    _remove_target(target);
  }

  cfg_schema_remove_section(olsr_cfg_get_schema(), &_interface_section);

  olsr_timer_remove(&_aggregation_timer);

  rfc5444_writer_cleanup(&_writer);
  rfc5444_reader_cleanup(&_reader);

  olsr_memcookie_remove(&_target_memcookie);
  olsr_memcookie_remove(&_tlvblock_memcookie);
  olsr_memcookie_remove(&_addrblock_memcookie);
  olsr_memcookie_remove(&_address_memcookie);
  olsr_memcookie_remove(&_addrtlv_memcookie);
  return;
}

/**
 * Get access to an existing RFC5444 interface handler. This also increase
 * the reference counter of
 * @param name interface name
 * @return pointer to RFC5444 target, NULL if an error happened
 */
struct olsr_rfc5444_target *
olsr_rfc5444_get_mc_target(const char *name) {
  struct olsr_rfc5444_target *target;

  return avl_find_element(&_targets_tree, name, target, _node);
}

/**
 * Release an active RFC5444 handler
 * @param target pointer to RFC5444 target
 */
static void
_remove_target(struct olsr_rfc5444_target *target) {
  /* stop aggregation timer */
  olsr_timer_stop(&target->_aggregation_v4);
  olsr_timer_stop(&target->_aggregation_v6);

  /* unhook interface listener */
  olsr_interface_remove_listener(&target->_if_listener);

  /* remove from avl tree */
  avl_remove(&_targets_tree, &target->_node);

  /* disable rfc5444 interface */
  rfc5444_writer_unregister_interface(&_writer, &target->if_ipv4);
  rfc5444_writer_unregister_interface(&_writer, &target->if_ipv6);

  /* free memory */
  olsr_memcookie_free(&_target_memcookie, target);
}

/**
 * @return pointer to global rfc5444 reader
 */
struct rfc5444_reader *
olsr_rfc5444_get_reader(void) {
  return &_reader;
}

/**
 * @return pointer to global rfc5444 writer
 */
struct rfc5444_writer *
olsr_rfc5444_get_writer(void) {
  return &_writer;
}

/**
 * This function allows access to the source address of a
 * RFC5444 packet which is currently parsed.
 * @return socket of incoming packet,
 */
const union netaddr_socket *
olsr_rfc5444_get_source_address(void) {
  return _current_source;
}

/**
 * Trigger the creation of a RFC5444 message
 * @param target interface for outgoing message, NULL for all interfaces
 * @param msgid id of created message
 * @param ipv4 true if message should send with IPv4
 * @param ipv6 true if message should send with IPv6
 * @return return code of rfc5444 writer
 */
enum rfc5444_result olsr_rfc5444_send(
    struct olsr_rfc5444_target *target, uint8_t msgid, bool ipv4, bool ipv6) {
  /* store IP selection in session variables */
  _send_ipv4 = ipv4;
  _send_ipv6 = ipv6;

  if (ipv4 && !olsr_timer_is_active(&target->_aggregation_v4)) {
    olsr_timer_start(&target->_aggregation_v4, _aggregation_interval);
  }
  if (ipv6 && !olsr_timer_is_active(&target->_aggregation_v6)) {
    olsr_timer_start(&target->_aggregation_v6, _aggregation_interval);
  }
  return rfc5444_writer_create_message(&_writer, msgid, _cb_writer_ifselector, target);
}


static struct olsr_rfc5444_target *
_create_target(const char *name) {
  struct olsr_rfc5444_target *target;

  target = avl_find_element(&_targets_tree, name, target, _node);
  if (target == NULL) {
    target = olsr_memcookie_malloc(&_target_memcookie);
    if (target == NULL) {
      return NULL;
    }

    /* initialize rfc5444 interfaces */
    target->if_ipv4.packet_buffer =
        ((uint8_t*)target) + sizeof(*target);
    target->if_ipv4.packet_size = MAX_PACKET_SIZE;
    target->if_ipv4.addPacketHeader = _cb_add_seqno;
    target->if_ipv4.sendPacket = _cb_send_packet_v4;
    target->if_ipv4.last_seqno = random() & 0xffff;
    rfc5444_writer_register_interface(&_writer, &target->if_ipv4);

    target->if_ipv6.packet_buffer =
        ((uint8_t*)target) + sizeof(*target) + MAX_PACKET_SIZE;
    target->if_ipv6.packet_size = MAX_PACKET_SIZE;
    target->if_ipv6.addPacketHeader = _cb_add_seqno;
    target->if_ipv6.sendPacket = _cb_send_packet_v6;
    target->if_ipv6.last_seqno = random() & 0xffff;
    rfc5444_writer_register_interface(&_writer, &target->if_ipv6);

    /* avl node */
    target->_node.key = target->name;
    avl_insert(&_targets_tree, &target->_node);

    /* interface socket */
    olsr_packet_add_managed(&target->_socket);

    /* interface listener */
    target->_if_listener.name = target->name;
    olsr_interface_add_listener(&target->_if_listener);

    /* aggregation timer */
    target->_aggregation_v4.info = &_aggregation_timer;
    target->_aggregation_v4.cb_context = &target->if_ipv4;

    target->_aggregation_v6.info = &_aggregation_timer;
    target->_aggregation_v6.cb_context = &target->if_ipv6;
  }
  return target;
}

/**
 * Handle incoming packet from a socket
 * @param sock pointer to packet socket
 * @param from originator of incoming packet
 * @param length length of incoming packet
 */
static void
_cb_receive_data(struct olsr_packet_socket *sock,
      union netaddr_socket *from, size_t length) {
  enum rfc5444_result result;
  struct netaddr_str buf;

  _current_source = from;
  result = rfc5444_reader_handle_packet(&_reader, sock->config.input_buffer, length);
  if (result) {
    OLSR_WARN(LOG_RFC5444, "Error while parsing incoming packet from %s: %s (%d)",
        netaddr_socket_to_string(&buf, from), rfc5444_strerror(result), result);
  }
}

/**
 * Callback for sending an ipv4 packet to a rfc5444 target
 * @param writer rfc5444 writer
 * @param interf rfc5444 interface
 * @param ptr pointer to outgoing buffer
 * @param size_t length of buffer
 */
static void
_cb_send_packet_v4(struct rfc5444_writer *writer __attribute__((unused)),
    struct rfc5444_writer_interface *interf, void *ptr, size_t len) {
  struct olsr_rfc5444_target *target;

  target = container_of(interf, struct olsr_rfc5444_target, if_ipv4);
  olsr_packet_send_managed_multicast(&target->_socket, ptr, len, AF_INET);
}

/**
 * Callback for sending an ipv6 packet to a rfc5444 target
 * @param writer rfc5444 writer
 * @param interf rfc5444 interface
 * @param ptr pointer to outgoing buffer
 * @param size_t length of buffer
 */
static void
_cb_send_packet_v6(struct rfc5444_writer *writer __attribute__((unused)),
    struct rfc5444_writer_interface *interf, void *ptr, size_t len) {
  struct olsr_rfc5444_target *target;

  target = container_of(interf, struct olsr_rfc5444_target, if_ipv6);
  olsr_packet_send_managed_multicast(&target->_socket, ptr, len, AF_INET6);
}

static void
_cb_forward_message(
    struct rfc5444_reader_tlvblock_context *context,
    uint8_t *buffer, size_t length) {
  enum rfc5444_result result;

  if (!context->has_origaddr || !context->has_seqno) {
    /* do not forward messages that cannot run through the duplicate check */
    return;
  }

  // TODO: handle duplicate detection

  // TODO: handle MPRs

  result = rfc5444_writer_forward_msg(&_writer, buffer, length,
      _cb_forward_ifselector, NULL);
  if (result) {
    OLSR_WARN(LOG_RFC5444, "Error while forwarding message: %s (%d)",
        rfc5444_strerror(result), result);
  }
}

/**
 * Selector for outgoing interfaces
 * @param writer rfc5444 writer
 * @param interf rfc5444 interface
 * @param ptr custom pointer, contains rfc5444 target
 *   or NULL if all interfaces
 * @return true if ptr is NULL or interface corresponds to custom pointer
 */
static bool
_cb_writer_ifselector(struct rfc5444_writer *writer __attribute__((unused)),
    struct rfc5444_writer_interface *interf, void *ptr) {
  struct olsr_rfc5444_target *target;

  target = ptr;
  return ptr == NULL
      || (_send_ipv4 && interf == &target->if_ipv4)
      || (_send_ipv6 && interf == &target->if_ipv6);
}

/**
 * Selector for forwarding interfaces
 * @param writer rfc5444 writer
 * @param interf rfc5444 interface
 * @param ptr NULL in this case
 * @return true if ptr is NULL or interface corresponds to custom pointer
 */
static bool
_cb_forward_ifselector(struct rfc5444_writer *writer __attribute__((unused)),
    struct rfc5444_writer_interface *interf __attribute__((unused)),
    void *ptr __attribute__((unused))) {
  // TODO: select forwarding interfaces
  return true;
}

/**
 * Internal memory allocation function for addrblock
 * @return pointer to cleared addrblock
 */
static struct rfc5444_reader_addrblock_entry *
_alloc_addrblock_entry(void) {
  return olsr_memcookie_malloc(&_addrblock_memcookie);
}

/**
 * Internal memory allocation function for rfc5444_reader_tlvblock_entry
 * @return pointer to cleared rfc5444_reader_tlvblock_entry
 */
static struct rfc5444_reader_tlvblock_entry *
_alloc_tlvblock_entry(void) {
  return olsr_memcookie_malloc(&_tlvblock_memcookie);
}

/**
 * Internal memory allocation function for rfc5444_writer_address
 * @return pointer to cleared rfc5444_writer_address
 */
static struct rfc5444_writer_address *
_alloc_address_entry(void) {
  return olsr_memcookie_malloc(&_address_memcookie);
}

/**
 * Internal memory allocation function for rfc5444_writer_addrtlv
 * @return pointer to cleared rfc5444_writer_addrtlv
 */
static struct rfc5444_writer_addrtlv *
_alloc_addrtlv_entry(void) {
  return olsr_memcookie_malloc(&_addrtlv_memcookie);
}

/**
 * Free an addrblock entry
 * @param pointer to addrblock
 */
static void
_free_addrblock_entry(void *addrblock) {
  olsr_memcookie_free(&_addrblock_memcookie, addrblock);
}

/**
 * Free a tlvblock entry
 * @param pointer to tlvblock
 */
static void
_free_tlvblock_entry(void *tlvblock) {
  olsr_memcookie_free(&_tlvblock_memcookie, tlvblock);
}

/**
 * Free a tlvblock entry
 * @param pointer to tlvblock
 */
static void
_free_address_entry(void *address) {
  olsr_memcookie_free(&_address_memcookie, address);
}

/**
 * Free a tlvblock entry
 * @param pointer to tlvblock
 */
static void
_free_addrtlv_entry(void *addrtlv) {
  olsr_memcookie_free(&_addrtlv_memcookie, addrtlv);
}

/**
 * Callback to add sequence number to outgoing RFC5444 packet
 * @param writer pointer to rfc5444 writer
 * @param interf pointer to rfc5444 interface
 */
static void
_cb_add_seqno(struct rfc5444_writer *writer, struct rfc5444_writer_interface *interf) {
  rfc5444_writer_set_pkt_header(writer, interf, true);
  rfc5444_writer_set_pkt_seqno(writer, interf, interf->last_seqno + 1);
}

/**
 * Timer callback for message aggregation
 * @param ptr pointer to rfc5444 target
 */
static void
_cb_aggregation_event (void *ptr) {
  struct rfc5444_writer_interface *interf;

  interf = ptr;

  rfc5444_writer_flush(&_writer, interf, false);
}

/**
 * Configuration has changed, handle the changes
 */
static void
_cb_config_changed(void) {
  struct _interface_config config;
  struct olsr_rfc5444_target *target;
  int result;

  if ((_interface_section.post != NULL && cfg_db_is_named_section(_interface_section.post))
      || (_interface_section.pre != NULL && cfg_db_is_named_section(_interface_section.pre))) {
    /* ignore unnamed section, they are only for delivering defaults */
    return;
  }
  if (_interface_section.post == NULL) {
    /* this section has been removed */
    target = olsr_rfc5444_get_mc_target(
        _interface_section.pre->name);
    if (target == NULL) {
      OLSR_WARN(LOG_RFC5444, "Warning, unknown "_CFG_SECTION" section '%s' was removed",
          _interface_section.pre->name);
      return;
    }

    _remove_target(target);
    return;
  }

  memset(&config, 0, sizeof(config));
  result = cfg_schema_tobin(&config, _interface_section.post,
      _interface_entries, ARRAYSIZE(_interface_entries));
  if (result) {
    OLSR_WARN(LOG_RFC5444, "Could not convert interface config to binary (%d)", -(result+1));
    return;
  }

  /* set interface name in socket config */
  strscpy(config.socket.interface, _interface_section.post->name,
      sizeof(config.socket.interface));

  if (_interface_section.pre == NULL) {
    /* section has been added */
    target = _create_target(_interface_section.post->name);
  }
  else {
    /* section has been changed */
    target = olsr_rfc5444_get_mc_target(_interface_section.post->name);
  }

  if (target == NULL) {
    OLSR_WARN(LOG_RFC5444, "Warning, unknown "_CFG_SECTION" section '%s'",
        _interface_section.post->name);
    return;
  }

  olsr_packet_apply_managed(&target->_socket, &config.socket);

  _aggregation_interval = config.aggregation_interval;
}

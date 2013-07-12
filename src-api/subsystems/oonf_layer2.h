
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

#ifndef OONF_LAYER2_H_
#define OONF_LAYER2_H_

#include "common/avl.h"
#include "common/common_types.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_timer.h"

#define LAYER2_CLASS_NEIGHBOR  "layer2_neighbor"
#define LAYER2_CLASS_NETWORK   "layer2_network"

#define OONF_LAYER2_NET_MAX_BITRATE_KEY  "max_bitrate"
#define OONF_LAYER2_NET_FREQUENCY_KEY    "frequency"

#define OONF_LAYER2_NEIGH_SIGNAL_KEY     "signal"
#define OONF_LAYER2_NEIGH_TX_BITRATE_KEY "tx_bitrate"
#define OONF_LAYER2_NEIGH_RX_BITRATE_KEY "rx_bitrate"
#define OONF_LAYER2_NEIGH_TX_BYTES_KEY   "tx_bytes"
#define OONF_LAYER2_NEIGH_RX_BYTES_KEY   "rx_bytes"
#define OONF_LAYER2_NEIGH_TX_FRAMES_KEY  "tx_frames"
#define OONF_LAYER2_NEIGH_RX_FRAMES_KEY  "rx_frames"
#define OONF_LAYER2_NEIGH_TX_RETRIES_KEY "tx_retries"
#define OONF_LAYER2_NEIGH_TX_FAILED_KEY  "tx_failed"

struct oonf_layer2_data {
  int64_t _value;
  bool _has_value;
  uint32_t _origin;
};

enum oonf_layer2_network_index {
  OONF_LAYER2_NET_FREQUENCY,
  OONF_LAYER2_NET_MAX_BITRATE,

  /* last entry */
  OONF_LAYER2_NET_COUNT,
};

enum oonf_layer2_network_type {
  OONF_LAYER2_TYPE_UNDEFINED,
  OONF_LAYER2_TYPE_WIRELESS,
  OONF_LAYER2_TYPE_ETHERNET,
  OONF_LAYER2_TYPE_TUNNEL,
};

enum oonf_layer2_neighbor_index {
  OONF_LAYER2_NEIGH_SIGNAL,
  OONF_LAYER2_NEIGH_TX_BITRATE,
  OONF_LAYER2_NEIGH_RX_BITRATE,
  OONF_LAYER2_NEIGH_TX_BYTES,
  OONF_LAYER2_NEIGH_RX_BYTES,
  OONF_LAYER2_NEIGH_TX_FRAMES,
  OONF_LAYER2_NEIGH_RX_FRAMES,
  OONF_LAYER2_NEIGH_TX_RETRIES,
  OONF_LAYER2_NEIGH_TX_FAILED,

  /* last entry */
  OONF_LAYER2_NEIGH_COUNT,
};

struct oonf_layer2_net {
  struct avl_node _node;
  struct netaddr addr;

  int  if_index;
  char if_name[IF_NAMESIZE];
  char if_ident[64];
  enum oonf_layer2_network_type if_type;

  struct avl_tree neighbors;
  struct avl_tree _ip_defaults;

  uint64_t last_seen;

  struct oonf_layer2_data data[OONF_LAYER2_NET_COUNT];
  struct oonf_layer2_data neighdata[OONF_LAYER2_NEIGH_COUNT];
};

struct oonf_layer2_neigh {
  struct avl_node _node;
  struct netaddr addr;

  struct list_entity _neigh_ring;

  struct oonf_layer2_net *network;

  uint64_t last_seen;

  struct oonf_layer2_data data[OONF_LAYER2_NEIGH_COUNT];
};

struct oonf_layer2_metadata {
  const char key[16];
  const char unit[8];
  const int fraction;
  const bool binary;
};

#define LOG_LAYER2 oonf_layer2_subsystem.logging
EXPORT extern struct oonf_subsystem oonf_layer2_subsystem;

EXPORT extern const struct oonf_layer2_metadata oonf_layer2_metadata_neigh[OONF_LAYER2_NEIGH_COUNT];
EXPORT extern const struct oonf_layer2_metadata oonf_layer2_metadata_net[OONF_LAYER2_NET_COUNT];

EXPORT extern struct avl_tree oonf_layer2_net_tree;

EXPORT uint32_t oonf_layer2_register_origin(void);
EXPORT void oonf_layer2_cleanup_origin(uint32_t);

EXPORT struct oonf_layer2_net *oonf_layer2_net_add(struct netaddr *);
EXPORT void oonf_layer2_net_remove(
    struct oonf_layer2_net *, uint32_t origin);
EXPORT bool oonf_layer2_net_commit(struct oonf_layer2_net *);

EXPORT struct oonf_layer2_neigh *oonf_layer2_neigh_add(
    struct oonf_layer2_net *, struct netaddr *l2neigh);
EXPORT void oonf_layer2_neigh_remove(
    struct oonf_layer2_neigh *l2neigh, uint32_t origin);
EXPORT bool oonf_layer2_neigh_commit(struct oonf_layer2_neigh *l2neigh);

EXPORT const struct oonf_layer2_data *oonf_layer2_neigh_query(
    const struct netaddr *l2net, const struct netaddr *l2neigh,
    enum oonf_layer2_neighbor_index idx);

/**
 * Get a layer-2 addr object from the database
 * @param addr local mac address of addr
 * @return layer-2 addr object, NULL if not found
 */
static inline struct oonf_layer2_net *
oonf_layer2_net_get(const struct netaddr *addr) {
  struct oonf_layer2_net *l2net;
  return avl_find_element(&oonf_layer2_net_tree, addr, l2net, _node);
}

/**
 * Get a layer-2 neighbor object from the database
 * @param l2net layer-2 addr object
 * @param addr remote mac address of neighbor
 * @return layer-2 neighbor object, NULL if not found
 */
static inline struct oonf_layer2_neigh *
oonf_layer2_neigh_get(const struct oonf_layer2_net *l2net,
    const struct netaddr *addr) {
  struct oonf_layer2_neigh *l2neigh;
  return avl_find_element(&l2net->neighbors, addr, l2neigh, _node);
}

/**
 * @param l2data layer-2 data object
 * @return true if object contains a value, false otherwise
 */
static inline bool
oonf_layer2_has_value(const struct oonf_layer2_data *l2data) {
  return l2data->_has_value;
}

/**
 * @param l2data layer-2 data object
 * @return value of data object
 */
static inline int64_t
oonf_layer2_get_value(const struct oonf_layer2_data *l2data) {
  return l2data->_value;
}

/**
 * @param l2data layer-2 data object
 * @return originator of data value
 */
static inline uint32_t
oonf_layer2_get_origin(const struct oonf_layer2_data *l2data) {
  return l2data->_origin;
}

/**
 * Set the value of a layer-2 data object
 * @param l2data layer-2 data object
 * @param origin originator of value
 * @param value new value for data object
 */
static inline void
oonf_layer2_set_value(struct oonf_layer2_data *l2data,
    uint32_t origin, int64_t value) {
  l2data->_has_value = true;
  l2data->_value = value;
  l2data->_origin = origin;
}

/**
 * Removes the value of a layer-2 data object
 * @param l2data layer-2 data object
 */
static inline void
oonf_layer2_reset_value(struct oonf_layer2_data *l2data) {
  l2data->_has_value = false;
  l2data->_origin = 0;
}

#endif /* OONF_LAYER2_H_ */

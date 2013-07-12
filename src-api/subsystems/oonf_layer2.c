
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

#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/common_types.h"
#include "common/netaddr.h"
#include "config/cfg_schema.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_layer2.h"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static bool _commit(struct oonf_layer2_net *l2net, bool commit_change);
static void _net_remove(struct oonf_layer2_net *l2net);
static void _neigh_remove(struct oonf_layer2_neigh *l2neigh);

/* subsystem definition */
struct oonf_subsystem oonf_layer2_subsystem = {
    .name = "layer2",
    .init = _init,
    .cleanup = _cleanup,
};

/* l2neigh string keys */
const struct oonf_layer2_metadata oonf_layer2_metadata_neigh[OONF_LAYER2_NEIGH_COUNT] = {
  [OONF_LAYER2_NEIGH_SIGNAL]     = { .key = OONF_LAYER2_NEIGH_SIGNAL_KEY, .unit = "dBm", .fraction = 1 },
  [OONF_LAYER2_NEIGH_TX_BITRATE] = { .key = OONF_LAYER2_NEIGH_TX_BITRATE_KEY, .unit = "bit/s", .binary = true },
  [OONF_LAYER2_NEIGH_RX_BITRATE] = { .key = OONF_LAYER2_NEIGH_RX_BITRATE_KEY, .unit = "bit/s", .binary = true },
  [OONF_LAYER2_NEIGH_TX_BYTES]   = { .key = OONF_LAYER2_NEIGH_TX_BYTES_KEY, .unit = "byte", .binary = true },
  [OONF_LAYER2_NEIGH_RX_BYTES]   = { .key = OONF_LAYER2_NEIGH_RX_BYTES_KEY, .unit = "byte", .binary = true },
  [OONF_LAYER2_NEIGH_TX_FRAMES]  = { .key = OONF_LAYER2_NEIGH_TX_FRAMES_KEY },
  [OONF_LAYER2_NEIGH_RX_FRAMES]  = { .key = OONF_LAYER2_NEIGH_RX_FRAMES_KEY },
  [OONF_LAYER2_NEIGH_TX_RETRIES] = { .key = OONF_LAYER2_NEIGH_TX_RETRIES_KEY },
  [OONF_LAYER2_NEIGH_TX_FAILED]  = { .key = OONF_LAYER2_NEIGH_TX_FAILED_KEY },
};

const struct oonf_layer2_metadata oonf_layer2_metadata_net[OONF_LAYER2_NET_COUNT] = {
  [OONF_LAYER2_NET_FREQUENCY]    = { .key = OONF_LAYER2_NET_FREQUENCY_KEY, .unit = "Hz" },
  [OONF_LAYER2_NET_MAX_BITRATE]  = { .key = OONF_LAYER2_NET_MAX_BITRATE_KEY, .unit = "bit/s", .binary = true },
};

/* infrastructure for l2net/l2neigh tree */
static struct oonf_class _l2network_class = {
  .name = LAYER2_CLASS_NETWORK,
  .size = sizeof(struct oonf_layer2_net),
};
static struct oonf_class _l2neighbor_class = {
  .name = LAYER2_CLASS_NEIGHBOR,
  .size = sizeof(struct oonf_layer2_neigh),
};

struct avl_tree oonf_layer2_net_tree;

static uint32_t _next_origin = 0;

/**
 * Subsystem constructor
 * @return always returns 0
 */
static int
_init(void) {
  oonf_class_add(&_l2network_class);
  oonf_class_add(&_l2neighbor_class);

  avl_init(&oonf_layer2_net_tree, avl_comp_netaddr, false);
  return 0;
}

/**
 * Subsystem destructor
 */
static void
_cleanup(void) {
  struct oonf_layer2_net *l2net, *l2n_it;

  avl_for_each_element_safe(&oonf_layer2_net_tree, l2net, _node, l2n_it) {
    _net_remove(l2net);
  }

  oonf_class_remove(&_l2neighbor_class);
  oonf_class_remove(&_l2network_class);
}

/**
 * Register a new data originator number for layer2 data
 * @return originator number
 */
uint32_t
oonf_layer2_register_origin(void) {
  _next_origin++;
  return _next_origin;
}

/**
 * Removes all layer2 data associated with this data originator
 * @param origin originator number
 */
void
oonf_layer2_cleanup_origin(uint32_t origin) {
  struct oonf_layer2_net *l2net, *l2net_it;

  avl_for_each_element_safe(&oonf_layer2_net_tree, l2net, _node, l2net_it) {
    oonf_layer2_net_remove(l2net, origin);
  }
}

/**
 * Add a layer-2 addr to the database
 * @param addr local mac address of addr
 * @return layer-2 addr object
 */
struct oonf_layer2_net *
oonf_layer2_net_add(struct netaddr *network) {
  struct oonf_layer2_net *l2net;

  l2net = avl_find_element(&oonf_layer2_net_tree, network, l2net, _node);
  if (l2net) {
    return l2net;
  }

  l2net = oonf_class_malloc(&_l2network_class);
  if (!l2net) {
    return NULL;
  }

  memcpy(&l2net->addr, network, sizeof(*network));
  l2net->_node.key = &l2net->addr;
  avl_insert(&oonf_layer2_net_tree, &l2net->_node);

  avl_init(&l2net->neighbors, avl_comp_netaddr, false);
  avl_init(&l2net->_ip_defaults, avl_comp_netaddr, false);

  oonf_class_event(&_l2network_class, l2net, OONF_OBJECT_ADDED);

  return l2net;
}

/**
 * Remove all information of a certain originator from a layer-2 addr
 * object. Remove the object if its empty and has no neighbors anymore.
 * @param l2net layer-2 addr object
 * @param origin originator number
 */
void
oonf_layer2_net_remove(struct oonf_layer2_net *l2net, uint32_t origin) {
  struct oonf_layer2_neigh *l2neigh, *l2neigh_it;
  int i;

  avl_for_each_element_safe(&l2net->neighbors, l2neigh, _node, l2neigh_it) {
    oonf_layer2_neigh_remove(l2neigh, origin);
  }

  for (i=0; i<OONF_LAYER2_NET_COUNT; i++) {
    if (l2net->data[i]._origin == origin) {
      oonf_layer2_reset_value(&l2net->data[i]);
    }
  }
  _commit(l2net, false);
}

/**
 * Commit all changes to a layer-2 addr object. This might remove the
 * object from the database if all data has been removed from the object.
 * @param l2net layer-2 addr object
 * @return true if the object has been removed, false otherwise
 */
bool
oonf_layer2_net_commit(struct oonf_layer2_net *l2net) {
  return _commit(l2net, true);
}

/**
 * Add a layer-2 neighbor to a addr.
 * @param l2net layer-2 addr object
 * @param neigh mac address of layer-2 neighbor
 * @return layer-2 neighbor object
 */
struct oonf_layer2_neigh *
oonf_layer2_neigh_add(struct oonf_layer2_net *l2net,
    struct netaddr *neigh) {
  struct oonf_layer2_neigh *l2neigh;

  if (netaddr_get_address_family(neigh) != AF_MAC48
      && netaddr_get_address_family(neigh) != AF_EUI64) {
    return NULL;
  }

  l2neigh = oonf_layer2_neigh_get(l2net, neigh);
  if (l2neigh) {
    return l2neigh;
  }

  l2neigh = oonf_class_malloc(&_l2neighbor_class);
  if (!l2neigh) {
    return NULL;
  }

  memcpy(&l2neigh->addr, neigh, sizeof(*neigh));
  l2neigh->_node.key = &l2neigh->addr;
  l2neigh->network = l2net;

  avl_insert(&l2net->neighbors, &l2neigh->_node);

  if (netaddr_get_address_family(neigh) == AF_MAC48
      || netaddr_get_address_family(neigh) == AF_EUI64) {
    /* initialize ring for IP addresses of neighbor */
    list_init_head(&l2neigh->_neigh_ring);
  }

  oonf_class_event(&_l2neighbor_class, l2neigh, OONF_OBJECT_ADDED);

  return l2neigh;
}

/**
 * Remove all information of a certain originator from a layer-2 neighbor
 * object. Remove the object if its empty.
 * @param l2neigh layer-2 neighbor object
 * @param origin originator number
 */
void
oonf_layer2_neigh_remove(struct oonf_layer2_neigh *l2neigh, uint32_t origin) {
  int i;

  for (i=0; i<OONF_LAYER2_NEIGH_COUNT; i++) {
    if (l2neigh->data[i]._origin == origin) {
      oonf_layer2_reset_value(&l2neigh->data[i]);
    }
  }
  oonf_layer2_neigh_commit(l2neigh);
}

/**
 * Commit all changes to a layer-2 neighbor object. This might remove the
 * object from the database if all data has been removed from the object.
 * @param l2neigh layer-2 neighbor object
 * @return true if the object has been removed, false otherwise
 */
bool
oonf_layer2_neigh_commit(struct oonf_layer2_neigh *l2neigh) {
  size_t i;

  for (i=0; i<OONF_LAYER2_NET_COUNT; i++) {
    if (oonf_layer2_has_value(&l2neigh->data[i])) {
      oonf_class_event(&_l2neighbor_class, l2neigh, OONF_OBJECT_CHANGED);
      return false;
    }
  }

  _neigh_remove(l2neigh);
  return true;
}

/**
 * Get neighbor specific data, either from neighbor or from the networks default
 * @param l2net_addr network mac address
 * @param l2neigh_addr neighbor mac address
 * @param idx data index
 * @return pointer to linklayer data, NULL if no value available
 */
const struct oonf_layer2_data *
oonf_layer2_neigh_query(const struct netaddr *l2net_addr,
    const struct netaddr *l2neigh_addr, enum oonf_layer2_neighbor_index idx) {
  struct oonf_layer2_net *l2net;
  struct oonf_layer2_neigh *l2neigh;
  struct oonf_layer2_data *data;

  /* query layer2 database about neighbor */
  l2net = oonf_layer2_net_get(l2net_addr);
  if (l2net == NULL) {
    return NULL;
  }

  l2neigh = oonf_layer2_neigh_get(l2net, l2neigh_addr);
  if (l2neigh == NULL) {
    data = &l2neigh->data[idx];
    if (oonf_layer2_has_value(data)) {
      return data;
    }
  }

  data = &l2neigh->network->neighdata[idx];
  if (oonf_layer2_has_value(data)) {
    return data;
  }
  return NULL;
}

/**
 * Commit all changes to a layer-2 addr object. This might remove the
 * object from the database if all data has been removed from the object.
 * @param l2net layer-2 addr object
 * @param commit_change true if function shall trigger change events
 * @return true if the object has been removed, false otherwise
 */
static bool
_commit(struct oonf_layer2_net *l2net, bool commit_change) {
  size_t i;

  if (l2net->neighbors.count > 0) {
    oonf_class_event(&_l2network_class, l2net, OONF_OBJECT_CHANGED);
    return false;
  }

  for (i=0; i<OONF_LAYER2_NET_COUNT; i++) {
    if (oonf_layer2_has_value(&l2net->data[i])) {
      if (commit_change) {
        oonf_class_event(&_l2network_class, l2net, OONF_OBJECT_CHANGED);
      }
      return false;
    }
  }

  _net_remove(l2net);
  return true;
}

/**
 * Removes a layer-2 addr object from the database.
 * @param l2net layer-2 addr object
 */
static void
_net_remove(struct oonf_layer2_net *l2net) {
  struct oonf_layer2_neigh *l2neigh, *l2n_it;

  /* free all embedded neighbors */
  avl_for_each_element_safe(&l2net->neighbors, l2neigh, _node, l2n_it) {
    _neigh_remove(l2neigh);
  }

  oonf_class_event(&_l2network_class, l2net, OONF_OBJECT_REMOVED);

  /* free addr */
  avl_remove(&oonf_layer2_net_tree, &l2net->_node);
  oonf_class_free(&_l2network_class, l2net);
}

/**
 * Removes a layer-2 neighbor object from the database
 * @param l2neigh layer-2 neighbor object
 */
static void
_neigh_remove(struct oonf_layer2_neigh *l2neigh) {
  struct oonf_layer2_neigh *neigh, *n_it;

  /* inform user that mac entry will be removed */
  oonf_class_event(&_l2neighbor_class, l2neigh, OONF_OBJECT_REMOVED);

  /* remove all connected IP defaults */
  list_for_each_element_safe(&l2neigh->_neigh_ring, neigh, _neigh_ring, n_it) {
    list_remove(&neigh->_neigh_ring);
  }

  /* free resources for mac entry */
  avl_remove(&l2neigh->network->neighbors, &l2neigh->_node);
  oonf_class_free(&_l2neighbor_class, l2neigh);
}

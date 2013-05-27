
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

#include "core/oonf_logging.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_rfc5444.h"
#include "subsystems/oonf_timer.h"

/* definitions and constants */
#define CFG_KEY_LINKSPEED "linkspeed"

/* prototypes */
static int _init(void);
static void _cleanup(void);

static void _remove_neighbor(struct oonf_layer2_neighbor *);
static void _remove_network(struct oonf_layer2_network *);
static void _cb_neighbor_timeout(void *ptr);
static void _cb_network_timeout(void *ptr);
static int _avl_comp_l2neigh(const void *k1, const void *k2);
static const char *_cb_get_neighbor_name(
    struct oonf_objectkey_str *, struct oonf_class *, void *);
static const char *_cb_get_network_name(
    struct oonf_objectkey_str *, struct oonf_class *, void *);

struct avl_tree oonf_layer2_network_id_tree;
struct avl_tree oonf_layer2_neighbor_tree;

static struct oonf_class _network_cookie = {
  .name = "layer2 networks",
  .size = sizeof(struct oonf_layer2_network),
  .to_keystring = _cb_get_network_name,
};

static struct oonf_class _neighbor_cookie = {
  .name = "layer2 neighbors",
  .size = sizeof(struct oonf_layer2_neighbor),
  .to_keystring = _cb_get_neighbor_name,
};

static struct oonf_timer_info _network_vtime_info = {
  .name = "layer2 network vtime",
  .callback = _cb_network_timeout,
  .periodic = true,
};

static struct oonf_timer_info _neighbor_vtime_info = {
  .name = "layer2 neighbor vtime",
  .callback = _cb_neighbor_timeout,
  .periodic = true,
};

struct oonf_subsystem oonf_layer2_subsystem = {
  .name = "layer2",
  .init = _init,
  .cleanup = _cleanup,
};

/**
 * Initialize layer2 subsystem
 * @return always returns 0
 */
static int
_init(void) {
  oonf_class_add(&_network_cookie);
  oonf_class_add(&_neighbor_cookie);

  oonf_timer_add(&_network_vtime_info);
  oonf_timer_add(&_neighbor_vtime_info);

  avl_init(&oonf_layer2_network_id_tree, avl_comp_netaddr, false);
  avl_init(&oonf_layer2_neighbor_tree, _avl_comp_l2neigh, false);
  return 0;
}

/**
 * Cleanup all resources allocated by layer2 subsystem
 */
static void
_cleanup(void) {
  struct oonf_layer2_neighbor *neigh, *neigh_it;
  struct oonf_layer2_network *net, *net_it;

  avl_for_each_element_safe(&oonf_layer2_network_id_tree, net, _id_node, net_it) {
    net->active = false;
    oonf_layer2_remove_network(net);
  }

  avl_for_each_element_safe(&oonf_layer2_neighbor_tree, neigh, _node, neigh_it) {
    neigh->active = false;
    oonf_layer2_remove_neighbor(neigh);
  }

  oonf_timer_remove(&_network_vtime_info);
  oonf_timer_remove(&_neighbor_vtime_info);
  oonf_class_remove(&_network_cookie);
  oonf_class_remove(&_neighbor_cookie);
}

/**
 * Add an active network to the database. If an entry for the
 * interface does already exists, it will be returned by this
 * function and no new entry will be created.
 * @param radio_id ID of the radio (might be NULL)
 * @param if_index local interface index of network
 * @param name interface name of the radio (might be NULL)
 * @param vtime validity time of data
 * @return pointer to layer2 network data, NULL if OOM
 */
struct oonf_layer2_network *
oonf_layer2_add_network(struct netaddr *radio_id, uint32_t if_index,
    uint64_t vtime) {
  struct oonf_layer2_network *net;

  net = oonf_layer2_get_network_by_id(radio_id);
  if (!net) {
    net = oonf_class_malloc(&_network_cookie);
    if (!net) {
      return NULL;
    }

    /* initialize the nodes */
    net->_id_node.key = &net->radio_id;
    memcpy (&net->radio_id, radio_id, sizeof(*radio_id));
    avl_insert(&oonf_layer2_network_id_tree, &net->_id_node);

    net->if_index = if_index;
    net->_valitity_timer.info = &_network_vtime_info;
    net->_valitity_timer.cb_context = net;

    oonf_class_event(&_network_cookie, net, OONF_OBJECT_ADDED);
  }

  OONF_DEBUG(LOG_LAYER2, "Reset validity of network timer: %"PRIu64,
      vtime);
  net->active = true;
  oonf_timer_set(&net->_valitity_timer, vtime);
  return net;
}

/**
 * Remove a layer2 network from the database
 * @param net pointer to layer2 network data
 */
void
oonf_layer2_remove_network(struct oonf_layer2_network *net) {
  if (net->active) {
    /* restart validity timer */
    oonf_timer_set(&net->_valitity_timer,
      oonf_timer_get_period(&net->_valitity_timer));
  }
  _remove_network(net);
}

/**
 * Retrieve a layer2 neighbor from the database
 * @param radio_id pointer to radio_id of network
 * @param neigh_mac pointer to layer2 address of neighbor
 * @return pointer to layer2 neighbor data, NULL if not found
 */
struct oonf_layer2_neighbor *
oonf_layer2_get_neighbor(struct netaddr *radio_id, struct netaddr *neigh_mac) {
  struct oonf_layer2_neighbor_key key;
  struct oonf_layer2_neighbor *neigh;

  key.radio_mac = *radio_id;
  key.neighbor_mac = *neigh_mac;

  return avl_find_element(&oonf_layer2_neighbor_tree, &key, neigh, _node);
}

/**
 * Add a layer2 neighbor to the database. If an entry for the
 * neighbor on the interface does already exists, it will be
 * returned by this function and no new entry will be created.
 * @param radio_id pointer to radio_id of network
 * @param neigh_mac layer2 address of neighbor
 * @param if_index local interface index of the neighbor
 * @param vtime validity time of data
 * @return pointer to layer2 neighbor data, NULL if OOM
 */
struct oonf_layer2_neighbor *
oonf_layer2_add_neighbor(struct netaddr *radio_id, struct netaddr *neigh_mac,
    uint32_t if_index, uint64_t vtime) {
  struct oonf_layer2_neighbor *neigh;

  assert (vtime > 0);

  neigh = oonf_layer2_get_neighbor(radio_id, neigh_mac);
  if (!neigh) {
    neigh = oonf_class_malloc(&_neighbor_cookie);
    if (!neigh) {
      return NULL;
    }

    neigh->if_index = if_index;
    memcpy(&neigh->key.radio_mac, radio_id, sizeof(*radio_id));
    memcpy(&neigh->key.neighbor_mac, neigh_mac, sizeof(*neigh_mac));

    neigh->_node.key = &neigh->key;
    neigh->_valitity_timer.info = &_neighbor_vtime_info;
    neigh->_valitity_timer.cb_context = neigh;

    avl_insert(&oonf_layer2_neighbor_tree, &neigh->_node);
    oonf_class_event(&_neighbor_cookie, neigh, OONF_OBJECT_ADDED);
  }

  neigh->active = true;
  oonf_timer_set(&neigh->_valitity_timer, vtime);
  return neigh;
}

/**
 * Remove a layer2 neighbor from the database
 * @param neigh pointer to layer2 neighbor
 */
void
oonf_layer2_remove_neighbor(struct oonf_layer2_neighbor *neigh) {
  if (neigh->active) {
    /* restart validity timer */
    oonf_timer_set(&neigh->_valitity_timer,
        oonf_timer_get_period(&neigh->_valitity_timer));
  }
  _remove_neighbor(neigh);
}

/**
 * Set a new list of supported rates. Data will not be changed if an
 * error happens.
 * @param net pointer to layer2 network
 * @param rate_array pointer to array of supported rates
 * @param rate_count number of supported rates
 * @return -1 if an out of memory error happened, 0 otherwise.
 */
int
oonf_layer2_network_set_supported_rates(struct oonf_layer2_network *net,
    uint64_t *rate_array, size_t rate_count) {
  uint64_t *rates;

  rates = realloc(net->supported_rates, rate_count * sizeof(uint64_t));
  if (rates == NULL) {
    return -1;
  }

  net->_available_data |= OONF_L2NET_SUPPORTED_RATES;
  net->supported_rates = rates;
  net->rate_count = rate_count;
  memcpy(rates, rate_array, sizeof(uint64_t) * rate_count);

  return 0;
}

/**
 * Triggers a change callback for a layer2 neighbor
 * @param neigh pointer to layer2 neighbor
 */
void
oonf_layer2_neighbor_commit(struct oonf_layer2_neighbor *neigh) {
  oonf_class_event(&_neighbor_cookie, neigh, OONF_OBJECT_CHANGED);
}

/**
 * Triggers a change callback for a layer2 network
 * @param net pointer to layer2 network
 */
void
oonf_layer2_network_commit(struct oonf_layer2_network *net) {
  oonf_class_event(&_network_cookie, net, OONF_OBJECT_CHANGED);
}

/**
 * Remove a layer2 neighbor from the database
 * @param neigh pointer to layer2 neighbor
 */
static void
_remove_neighbor(struct oonf_layer2_neighbor *neigh) {
  if (neigh->active) {
    oonf_class_event(&_neighbor_cookie, neigh, OONF_OBJECT_REMOVED);
    neigh->active = false;
    return;
  }
  avl_remove(&oonf_layer2_neighbor_tree, &neigh->_node);
  oonf_timer_stop(&neigh->_valitity_timer);
  oonf_class_free(&_neighbor_cookie, neigh);
}

/**
 * Remove a layer2 network from the database
 * @param net pointer to layer2 network data
 */
static void
_remove_network(struct oonf_layer2_network *net) {
  if (net->active) {
    oonf_class_event(&_network_cookie, net, OONF_OBJECT_REMOVED);
    net->active = false;
    return;
  }

  avl_remove(&oonf_layer2_network_id_tree, &net->_id_node);

  oonf_timer_stop(&net->_valitity_timer);
  free (net->supported_rates);
  oonf_class_free(&_network_cookie, net);
}

/**
 * Validity time callback for neighbor entries
 * @param ptr pointer to neighbor entry
 */
static void
_cb_neighbor_timeout(void *ptr) {
#ifdef OONF_LOG_DEBUG_INFO
  struct oonf_layer2_neighbor *neigh = ptr;
#endif
  OONF_DEBUG(LOG_LAYER2, "Layer-2 neighbor timeout (was %sactive)", neigh->active ? "" : "in");
  _remove_neighbor(ptr);
}

/**
 * Validity time callback for network entries
 * @param ptr pointer to network entry
 */
static void
_cb_network_timeout(void *ptr) {
#ifdef OONF_LOG_DEBUG_INFO
  struct oonf_layer2_network *net = ptr;
#endif
  OONF_DEBUG(LOG_LAYER2, "Layer-2 network timeout (was %sactive)", net->active ? "" : "in");
  _remove_network(ptr);
}

/**
 * AVL comparator for layer2 neighbor nodes
 * @param k1 pointer to first layer2 neighbor
 * @param k2 pointer to second layer2 neighbor
 * @param ptr unused
 * @return +1 if k1>k2, -1 if k1<k2, 0 if k1==k2
 */
static int
_avl_comp_l2neigh(const void *k1, const void *k2) {
  const struct oonf_layer2_neighbor_key *key1, *key2;
  int result;

  key1 = k1;
  key2 = k2;

  result = netaddr_cmp(&key1->radio_mac, &key2->radio_mac);
  if (!result) {
    result = netaddr_cmp(&key1->neighbor_mac, &key2->neighbor_mac);
  }
  return result;
}

/**
 * Construct human readable object id of neighbor for callbacks
 * @param buf pointer to key output buffer
 * @param cl pointer to class
 * @param ptr pointer to l2 neighbor
 * @return pointer to id
 */
static const char *
_cb_get_neighbor_name(struct oonf_objectkey_str *buf,
    struct oonf_class *cl, void *ptr) {
  struct netaddr_str nbuf1, nbuf2;
  struct oonf_layer2_neighbor *nbr;

  nbr = ptr;

  snprintf(buf->buf, sizeof(*buf), "%s::neigh=%s/radio=%s",
      cl->name,
      netaddr_to_string(&nbuf1, &nbr->key.neighbor_mac),
      netaddr_to_string(&nbuf2, &nbr->key.radio_mac));
  return buf->buf;
}

/**
 * Construct human readable object id of network for callbacks
 * @param buf pointer to key output buffer
 * @param cl pointer to class
 * @param ptr pointer to l2 neighbor
 * @return pointer to id
 */
static const char *
_cb_get_network_name(struct oonf_objectkey_str *buf,
    struct oonf_class *cl, void *ptr) {
  struct netaddr_str buf1;
  struct oonf_layer2_network *net;

  net = ptr;

  snprintf(buf->buf, sizeof(*buf), "%s::radio=%s",
      cl->name, netaddr_to_string(&buf1, &net->radio_id));
  return buf->buf;
}

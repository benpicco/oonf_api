
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

#ifndef OONF_LINKCONFIG_H_
#define OONF_LINKCONFIG_H_

#include "common/avl.h"
#include "common/common_types.h"
#include "common/netaddr.h"
#include "core/oonf_subsystem.h"

/* both callbacks support ADD and REMOVE events */
#define LAYER2_CONFIG_CLASS_NEIGHBOR           "linkconfig_neighbor"
#define LAYER2_CONFIG_CLASS_NETWORK            "linkconfig_neighbor"

struct oonf_linkconfig_data {
  uint64_t tx_bitrate;
};
struct oonf_linkconfig_network {
  char name[IF_NAMESIZE];

  struct oonf_linkconfig_data data;

  struct avl_node _node;

  struct avl_tree _link_tree;
};

struct oonf_linkconfig_link {
  struct netaddr remote_mac;

  struct oonf_linkconfig_network *net;

  struct oonf_linkconfig_data data;

  struct avl_node _node;
};

#define CFG_VALIDATE_LINKSPEED(p_name, p_def, p_help, args...)         _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = oonf_linkconfig_validate_linkspeed, ##args )

EXPORT extern struct oonf_subsystem oonf_linkconfig_subsystem;
EXPORT extern struct avl_tree oonf_linkconfig_network_tree;
EXPORT extern const struct oonf_linkconfig_data oonf_linkconfig_default;

EXPORT struct oonf_linkconfig_network *oonf_linkconfig_network_add(
    const char *name);
EXPORT void oonf_linkconfig_network_remove(struct oonf_linkconfig_network *);

EXPORT struct oonf_linkconfig_link *oonf_linkconfig_link_add(
    struct oonf_linkconfig_network *, struct netaddr *remote);
EXPORT void oonf_linkconfig_link_remove(struct oonf_linkconfig_link *);

EXPORT int oonf_linkconfig_validate_linkspeed(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out);

/**
 * @param name interface name
 * @return user configured network data, NULL if not found
 */
static INLINE struct oonf_linkconfig_network *
oonf_linkconfig_network_get(const char *name) {
  struct oonf_linkconfig_network *net;

  return avl_find_element(&oonf_linkconfig_network_tree,
      name, net, _node);
}

/**
 * Returns the user configured link data for a certain neighbor
 * @param net pointer to user configured network
 * @param remote mac address of neighbor
 * @return user configured link data
 */
static INLINE struct oonf_linkconfig_link *
oonf_linkconfig_link_get(struct oonf_linkconfig_network *net,
    struct netaddr *remote) {
  struct oonf_linkconfig_link *lnk;

  return avl_find_element(&net->_link_tree, remote, lnk, _node);
}

/**
 * Returns the default link data set by the user
 * @param name interface name
 * @param remote remote network configuration
 * @return link data set for the neighbor, NULL if not found
 */
static INLINE const struct oonf_linkconfig_data *
oonf_linkconfig_get(const char *name, struct netaddr *remote) {
  struct oonf_linkconfig_network *net;
  struct oonf_linkconfig_link *lnk;

  net = oonf_linkconfig_network_get(name);
  if (!net) {
    return NULL;
  }

  lnk = oonf_linkconfig_link_get(net, remote);
  if (lnk) {
    return &lnk->data;
  }

  return &net->data;
}

#endif /* OONF_LINKCONFIG_H_ */

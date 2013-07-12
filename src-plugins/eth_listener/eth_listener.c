
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

#include <linux/sockios.h>
#include <net/if.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>

#include "common/common_types.h"
#include "config/cfg_schema.h"
#include "core/oonf_plugins.h"
#include "subsystems/oonf_clock.h"
#include "subsystems/oonf_interface.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_timer.h"

#include "eth_listener/eth_listener.h"
#include "eth_listener/ethtool-copy.h"

/* definitions */
struct _eth_config {
  uint64_t interval;
};

/* prototypes */
static int _init(void);
static void _cleanup(void);

static void _cb_transmission_event(void *);
static void _cb_config_changed(void);

/* configuration */
static struct cfg_schema_entry _eth_entries[] = {
  CFG_MAP_CLOCK_MIN(_eth_config, interval, "interval", "60.0",
      "Interval between two linklayer information updates", 100),
};

static struct cfg_schema_section _eth_section = {
  .type = OONF_PLUGIN_GET_NAME(),
  .cb_delta_handler = _cb_config_changed,
  .entries = _eth_entries,
  .entry_count = ARRAYSIZE(_eth_entries),
};

static struct _eth_config _config;

/* plugin declaration */
struct oonf_subsystem eth_listener_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONF ethernet listener plugin",
  .author = "Henning Rogge",

  .cfg_section = &_eth_section,

  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(eth_listener_subsystem);

/* timer for generating netlink requests */
static struct oonf_timer_info _transmission_timer_info = {
  .name = "nl80211 listener timer",
  .callback = _cb_transmission_event,
  .periodic = true,
};

static struct oonf_timer_entry _transmission_timer = {
  .info = &_transmission_timer_info
};

static int _ioctl_fd;
static uint32_t _l2_origin;

static int
_init(void) {
  _ioctl_fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (_ioctl_fd == -1) {
    return -1;
  }

  oonf_timer_add(&_transmission_timer_info);
  _l2_origin = oonf_layer2_register_origin();

  return 0;
}

static void
_cleanup(void) {
  oonf_layer2_cleanup_origin(_l2_origin);

  oonf_timer_stop(&_transmission_timer);
  oonf_timer_remove(&_transmission_timer_info);

  close (_ioctl_fd);
}

static void
_cb_transmission_event(void *ptr __attribute((unused))) {
  struct oonf_layer2_net *l2net;
  struct oonf_interface *interf;
  struct ethtool_cmd cmd;
  struct ifreq req;
  int64_t ethspeed;
  int err;

  avl_for_each_element(&oonf_interface_tree, interf, _node) {
    /* layer-2 object for this interface */
    l2net = oonf_layer2_net_get(&interf->data.mac);

    /* initialize ethtool command */
    memset(&cmd, 0, sizeof(cmd));
    cmd.cmd = ETHTOOL_GSET;

    /* initialize interface request */
    memset(&req, 0, sizeof(req));
    req.ifr_data = (void *)&cmd;
    strscpy(req.ifr_name, "eth0", IF_NAMESIZE);

    /* request ethernet information from kernel */
    err = ioctl(_ioctl_fd, SIOCETHTOOL, &req);
    if (err != 0) {
      if (l2net != NULL) {
        oonf_layer2_net_remove(l2net, _l2_origin);
      }
      continue;
    }

    if (l2net == NULL) {
      l2net = oonf_layer2_net_add(&interf->data.mac);
      if (l2net == NULL) {
        continue;
      }

      /* copy interface static values */
      l2net->if_index = interf->data.index;
      strscpy(l2net->if_name, interf->data.name, sizeof(l2net->if_name));
      l2net->if_type = OONF_LAYER2_TYPE_ETHERNET;
    }

    /* get ethernet linkspeed */
    ethspeed = ethtool_cmd_speed(&cmd);
    ethspeed *= 1000 * 1000;

    /* set corresponding database entries */
    oonf_layer2_set_value(&l2net->neighdata[OONF_LAYER2_NEIGH_RX_BITRATE],
        _l2_origin, ethspeed);
    oonf_layer2_set_value(&l2net->neighdata[OONF_LAYER2_NEIGH_TX_BITRATE],
        _l2_origin, ethspeed);
    oonf_layer2_set_value(&l2net->data[OONF_LAYER2_NET_MAX_BITRATE],
        _l2_origin, ethspeed);
  }
}

static void
_cb_config_changed(void) {
  if (cfg_schema_tobin(&_config, _eth_section.post,
      _eth_entries, ARRAYSIZE(_eth_entries))) {
    OONF_WARN(LOG_ETH, "Could not convert %s config to bin",
        OONF_PLUGIN_GET_NAME());
    return;
  }

  oonf_timer_set_ext(&_transmission_timer, 1, _config.interval);
}

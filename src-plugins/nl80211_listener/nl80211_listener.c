
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

#define _GNU_SOURCE

/* must be first because of a problem with linux/netlink.h */
#include <sys/socket.h>

/* and now the rest of the includes */
#include <linux/types.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include "nl80211.h"
#include <netlink/attr.h>
#include <netlink/msg.h>

#include "common/autobuf.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/common_types.h"
#include "common/netaddr.h"
#include "common/netaddr_acl.h"
#include "common/string.h"

#include "config/cfg.h"
#include "config/cfg_schema.h"
#include "core/oonf_logging.h"
#include "core/oonf_plugins.h"
#include "core/oonf_subsystem.h"
#include "subsystems/oonf_class.h"
#include "subsystems/oonf_clock.h"
#include "subsystems/oonf_interface.h"
#include "subsystems/oonf_layer2.h"
#include "subsystems/oonf_timer.h"
#include "subsystems/os_system.h"

#include "nl80211_listener/nl80211_listener.h"

/* definitions */
struct _nl80211_config {
  uint64_t interval;
};

enum query_type {
  QUERY_FIRST = 0,
  QUERY_STATION_DUMP = 0,
  QUERY_SCAN_DUMP,

  /* must be last */
  QUERY_COUNT,
};

/* prototypes */
static int _init(void);
static void _cleanup(void);

static void _cb_config_changed(void);
static void _send_genl_getfamily(void);

static void _cb_nl_message(struct nlmsghdr *hdr);
static void _cb_nl_error(uint32_t seq, int error);
static void _cb_nl_timeout(void);
static void _cb_nl_done(uint32_t seq);

static void _cb_transmission_event(void *);

/* configuration */
static struct cfg_schema_entry _nl80211_entries[] = {
  CFG_MAP_CLOCK_MIN(_nl80211_config, interval, "interval", "1.0",
      "Interval between two linklayer information updates", 100),
};

static struct cfg_schema_section _nl80211_section = {
  .type = OONF_PLUGIN_GET_NAME(),
  .cb_delta_handler = _cb_config_changed,
  .entries = _nl80211_entries,
  .entry_count = ARRAYSIZE(_nl80211_entries),
};

static struct _nl80211_config _config;

/* plugin declaration */
struct oonf_subsystem nl80211_listener_subsystem = {
  .name = OONF_PLUGIN_GET_NAME(),
  .descr = "OONF nl80211 listener plugin",
  .author = "Henning Rogge",

  .cfg_section = &_nl80211_section,

  .init = _init,
  .cleanup = _cleanup,
};
DECLARE_OONF_PLUGIN(nl80211_listener_subsystem);

/* netlink specific data */
static struct os_system_netlink _netlink_handler = {
  .cb_message = _cb_nl_message,
  .cb_error = _cb_nl_error,
  .cb_done = _cb_nl_done,
  .cb_timeout = _cb_nl_timeout,
};

static struct nlmsghdr *_msgbuf;

static int _nl80211_id = -1;
static bool _nl80211_mc_set = false;

static char _last_queried_if[IF_NAMESIZE];
static enum query_type _next_query_type;

/* timer for generating netlink requests */
static struct oonf_timer_info _transmission_timer_info = {
  .name = "nl80211 listener timer",
  .callback = _cb_transmission_event,
  .periodic = true,
};

struct oonf_timer_entry _transmission_timer = {
  .info = &_transmission_timer_info
};

/* logging source */
enum oonf_log_source LOG_NL80211;

/**
 * Constructor of plugin
 * @return 0 if initialization was successful, -1 otherwise
 */
static int
_init(void) {
  LOG_NL80211 = oonf_log_register_source(OONF_PLUGIN_GET_NAME());

  _msgbuf = calloc(1, UIO_MAXIOV);
  if (_msgbuf == NULL) {
    OONF_WARN(LOG_NL80211, "Not enough memory for nl80211 memory buffer");
    return -1;
  }

  if (os_system_netlink_add(&_netlink_handler, NETLINK_GENERIC)) {
    free(_msgbuf);
    return -1;
  }

  oonf_timer_add(&_transmission_timer_info);

  memset(_last_queried_if, 0, sizeof(_last_queried_if));
  _next_query_type = QUERY_STATION_DUMP;

  _send_genl_getfamily();
  return 0;
}

/**
 * Destructor of plugin
 */
static void
_cleanup(void) {
  oonf_timer_stop(&_transmission_timer);
  oonf_timer_remove(&_transmission_timer_info);
  os_system_netlink_remove(&_netlink_handler);

  free (_msgbuf);
}

/**
 * Parse the netlink message result that contains the list of available
 * generic netlink families of the kernel.
 * @param hdr pointer to netlink message
 */
static void
_parse_cmd_newfamily(struct nlmsghdr *hdr) {
  static struct nla_policy ctrl_policy[CTRL_ATTR_MAX+1] = {
    [CTRL_ATTR_FAMILY_ID]    = { .type = NLA_U16 },
    [CTRL_ATTR_FAMILY_NAME]  = { .type = NLA_STRING, .maxlen = GENL_NAMSIZ },
    [CTRL_ATTR_VERSION]      = { .type = NLA_U32 },
    [CTRL_ATTR_HDRSIZE]      = { .type = NLA_U32 },
    [CTRL_ATTR_MAXATTR]      = { .type = NLA_U32 },
    [CTRL_ATTR_OPS]          = { .type = NLA_NESTED },
    [CTRL_ATTR_MCAST_GROUPS] = { .type = NLA_NESTED },
  };
  struct nlattr *attrs[CTRL_ATTR_MAX+1];
  struct nlattr *mcgrp;
  int iterator;

  if (nlmsg_parse(hdr, sizeof(struct genlmsghdr),
      attrs, CTRL_ATTR_MAX, ctrl_policy) < 0) {
    OONF_WARN(LOG_NL80211, "Cannot parse netlink CTRL_CMD_NEWFAMILY message");
    return;
  }

  if (attrs[CTRL_ATTR_FAMILY_ID] == NULL) {
    OONF_WARN(LOG_NL80211, "Missing Family ID in CTRL_CMD_NEWFAMILY");
    return;
  }
  if (attrs[CTRL_ATTR_FAMILY_NAME] == NULL) {
    OONF_WARN(LOG_NL80211, "Missing Family Name in CTRL_CMD_NEWFAMILY");
    return;
  }
  if (strcmp(nla_get_string(attrs[CTRL_ATTR_FAMILY_NAME]), "nl80211") != 0) {
    /* not interested in this one */
    return;
  }
  _nl80211_id = nla_get_u32(attrs[CTRL_ATTR_FAMILY_ID]);

  if (_nl80211_mc_set || !attrs[CTRL_ATTR_MCAST_GROUPS]) {
    /* no multicast groups */
    return;
  }

  nla_for_each_nested(mcgrp, attrs[CTRL_ATTR_MCAST_GROUPS], iterator) {
    struct nlattr *tb_mcgrp[CTRL_ATTR_MCAST_GRP_MAX + 1];
    uint32_t group;

    nla_parse(tb_mcgrp, CTRL_ATTR_MCAST_GRP_MAX,
        nla_data(mcgrp), nla_len(mcgrp), NULL);

    if (!tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME] ||
        !tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID])
      continue;

    if (strcmp(nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]), "mlme"))
      continue;

    group = nla_get_u32(tb_mcgrp[CTRL_ATTR_MCAST_GRP_ID]);
    OONF_DEBUG(LOG_NL80211, "Found multicast group %s: %d",
        (char *)nla_data(tb_mcgrp[CTRL_ATTR_MCAST_GRP_NAME]),
        group);

    if (os_system_netlink_add_mc(&_netlink_handler, &group, 1)) {
      OONF_WARN(LOG_NL80211,
          "Could not activate multicast group %d for nl80211", group);
    }
    else {
      _nl80211_mc_set = true;
    }
    break;
  }
}

/**
 * Parse result of station dump nl80211 command
 * @param hdr pointer to netlink message
 */
static void
_parse_cmd_new_station(struct nlmsghdr *hdr) {
  static struct nla_policy stats_policy[NL80211_STA_INFO_MAX + 1] = {
    [NL80211_STA_INFO_INACTIVE_TIME] = { .type = NLA_U32 },
    [NL80211_STA_INFO_RX_BYTES]      = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_BYTES]      = { .type = NLA_U32 },
    [NL80211_STA_INFO_RX_PACKETS]    = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_PACKETS]    = { .type = NLA_U32 },
    [NL80211_STA_INFO_SIGNAL]        = { .type = NLA_U8 },
    [NL80211_STA_INFO_RX_BITRATE]    = { .type = NLA_NESTED },
    [NL80211_STA_INFO_TX_BITRATE]    = { .type = NLA_NESTED },
    [NL80211_STA_INFO_LLID]          = { .type = NLA_U16 },
    [NL80211_STA_INFO_PLID]          = { .type = NLA_U16 },
    [NL80211_STA_INFO_PLINK_STATE]   = { .type = NLA_U8 },
    [NL80211_STA_INFO_TX_RETRIES]    = { .type = NLA_U32 },
    [NL80211_STA_INFO_TX_FAILED]     = { .type = NLA_U32 },
  };
  static struct nla_policy rate_policy[NL80211_RATE_INFO_MAX + 1] = {
    [NL80211_RATE_INFO_BITRATE]      = { .type = NLA_U16 },
    [NL80211_RATE_INFO_MCS]          = { .type = NLA_U8 },
    [NL80211_RATE_INFO_40_MHZ_WIDTH] = { .type = NLA_FLAG },
    [NL80211_RATE_INFO_SHORT_GI]     = { .type = NLA_FLAG },
  };

  struct nlattr *tb[NL80211_ATTR_MAX + 1];
  struct nlattr *sinfo[NL80211_STA_INFO_MAX + 1];
  struct nlattr *rinfo[NL80211_RATE_INFO_MAX + 1];

  struct oonf_interface_data *if_data;
  struct oonf_layer2_neighbor *neigh;
  struct netaddr mac;
  unsigned if_index;
  char if_name[IF_NAMESIZE];

#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf1, buf2;
#endif

  if (nlmsg_parse(hdr, sizeof(struct genlmsghdr),
      tb, NL80211_ATTR_MAX, NULL) < 0) {
    OONF_WARN(LOG_NL80211, "Cannot parse netlink NL80211_CMD_NEW_STATION message");
    return;
  }

  if (!tb[NL80211_ATTR_STA_INFO]) {
    OONF_WARN(LOG_NL80211, "Cannot find station info attribute");
    return;
  }
  if (nla_parse_nested(sinfo, NL80211_STA_INFO_MAX,
           tb[NL80211_ATTR_STA_INFO], stats_policy)) {
    OONF_WARN(LOG_NL80211, "Cannot parse station info attribute");
    return;
  }

  netaddr_from_binary(&mac, nla_data(tb[NL80211_ATTR_MAC]), 6, AF_MAC48);
  if_index = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);

  if (if_indextoname(if_index, if_name) == NULL) {
    return;
  }

  if_data = oonf_interface_get_data(if_name, NULL);
  if (if_data == NULL || netaddr_get_address_family(&if_data->mac) == AF_UNSPEC) {
    return;
  }

  OONF_DEBUG(LOG_NL80211, "Add neighbor %s for network %s",
      netaddr_to_string(&buf1, &mac), netaddr_to_string(&buf2, &if_data->mac));

  neigh = oonf_layer2_add_neighbor(&if_data->mac, &mac, if_index,
      _config.interval + _config.interval / 4);
  if (neigh == NULL) {
    OONF_WARN(LOG_NL80211, "Not enough memory for new layer2 neighbor");
    return;
  }

  /* make sure that the network is there */
  oonf_layer2_add_network(&if_data->mac, if_index,
        _config.interval + _config.interval / 4);

  /* reset all existing data */
  oonf_layer2_neighbor_clear(neigh);

  /* insert new data */
  if (sinfo[NL80211_STA_INFO_INACTIVE_TIME]) {
    oonf_layer2_neighbor_set_last_seen(neigh,
        oonf_clock_get_absolute(nla_get_u32(sinfo[NL80211_STA_INFO_INACTIVE_TIME])));
  }
  if (sinfo[NL80211_STA_INFO_RX_BYTES]) {
    oonf_layer2_neighbor_set_rx_bytes(neigh,
        nla_get_u32(sinfo[NL80211_STA_INFO_RX_BYTES]));
  }
  if (sinfo[NL80211_STA_INFO_RX_PACKETS]) {
    oonf_layer2_neighbor_set_rx_packets(neigh,
        nla_get_u32(sinfo[NL80211_STA_INFO_RX_PACKETS]));
  }
  if (sinfo[NL80211_STA_INFO_TX_BYTES]) {
    oonf_layer2_neighbor_set_tx_bytes(neigh,
        nla_get_u32(sinfo[NL80211_STA_INFO_TX_BYTES]));
  }
  if (sinfo[NL80211_STA_INFO_TX_PACKETS]) {
    oonf_layer2_neighbor_set_tx_packets(neigh,
        nla_get_u32(sinfo[NL80211_STA_INFO_TX_PACKETS]));
  }
  if (sinfo[NL80211_STA_INFO_TX_RETRIES])  {
    oonf_layer2_neighbor_set_tx_retries(neigh,
        nla_get_u32(sinfo[NL80211_STA_INFO_TX_RETRIES]));
  }
  if (sinfo[NL80211_STA_INFO_TX_FAILED]) {
    oonf_layer2_neighbor_set_tx_fails(neigh,
        nla_get_u32(sinfo[NL80211_STA_INFO_TX_FAILED]));
  }
  if (sinfo[NL80211_STA_INFO_SIGNAL])  {
    oonf_layer2_neighbor_set_signal(neigh, (int8_t)nla_get_u8(sinfo[NL80211_STA_INFO_SIGNAL]));
  }
  if (sinfo[NL80211_STA_INFO_TX_BITRATE]) {
    if (nla_parse_nested(rinfo, NL80211_RATE_INFO_MAX,
             sinfo[NL80211_STA_INFO_TX_BITRATE], rate_policy) == 0) {
      if (rinfo[NL80211_RATE_INFO_BITRATE]) {
        uint64_t rate = nla_get_u16(rinfo[NL80211_RATE_INFO_BITRATE]);
        oonf_layer2_neighbor_set_tx_bitrate(neigh, ((uint64_t)rate * 1024 * 1024) / 10);
      }
      /* TODO: do we need the rest of the data ? */
#if 0
      if (rinfo[NL80211_RATE_INFO_MCS])
        printf(" MCS %d", nla_get_u8(rinfo[NL80211_RATE_INFO_MCS]));
      if (rinfo[NL80211_RATE_INFO_40_MHZ_WIDTH])
        printf(" 40Mhz");
      if (rinfo[NL80211_RATE_INFO_SHORT_GI])
        printf(" short GI");
#endif
    }
  }
  if (sinfo[NL80211_STA_INFO_RX_BITRATE]) {
    if (nla_parse_nested(rinfo, NL80211_RATE_INFO_MAX,
             sinfo[NL80211_STA_INFO_RX_BITRATE], rate_policy) == 0) {
      if (rinfo[NL80211_RATE_INFO_BITRATE]) {
        uint64_t rate = nla_get_u16(rinfo[NL80211_RATE_INFO_BITRATE]);
        oonf_layer2_neighbor_set_rx_bitrate(neigh, ((uint64_t)rate * 1024 * 1024) / 10);
      }
      /* TODO: do we need the rest of the data ? */
#if 0
      if (rinfo[NL80211_RATE_INFO_MCS])
        printf(" MCS %d", nla_get_u8(rinfo[NL80211_RATE_INFO_MCS]));
      if (rinfo[NL80211_RATE_INFO_40_MHZ_WIDTH])
        printf(" 40Mhz");
      if (rinfo[NL80211_RATE_INFO_SHORT_GI])
        printf(" short GI");
#endif
    }
  }

  oonf_layer2_neighbor_commit(neigh);
  return;
}

/**
 * Parse result of station dump nl80211 command
 * @param hdr pointer to netlink message
 */
static void
_parse_cmd_del_station(struct nlmsghdr *hdr) {
  struct nlattr *tb[NL80211_ATTR_MAX + 1];

  struct oonf_interface_data *if_data;
  struct oonf_layer2_neighbor *neigh;
  struct netaddr mac;
  unsigned if_index;
  char if_name[IF_NAMESIZE];
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf1, buf2;
#endif

  if (nlmsg_parse(hdr, sizeof(struct genlmsghdr),
      tb, NL80211_ATTR_MAX, NULL) < 0) {
    OONF_WARN(LOG_NL80211, "Cannot parse netlink NL80211_CMD_NEW_STATION message");
    return;
  }

  netaddr_from_binary(&mac, nla_data(tb[NL80211_ATTR_MAC]), 6, AF_MAC48);
  if_index = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);

  if (if_indextoname(if_index, if_name) == NULL) {
    return;
  }
  if_data = oonf_interface_get_data(if_name, NULL);
  if (if_data == NULL || netaddr_get_address_family(&if_data->mac) == AF_UNSPEC) {
    return;
  }

  OONF_DEBUG(LOG_NL80211, "Remove neighbor %s for network %s",
      netaddr_to_string(&buf1, &mac), netaddr_to_string(&buf2, &if_data->mac));

  neigh = oonf_layer2_get_neighbor(&if_data->mac, &mac);
  if (neigh != NULL) {
    oonf_layer2_remove_neighbor(neigh);
  }
}

#define WLAN_CAPABILITY_ESS   (1<<0)
#define WLAN_CAPABILITY_IBSS    (1<<1)
#define WLAN_CAPABILITY_CF_POLLABLE (1<<2)
#define WLAN_CAPABILITY_CF_POLL_REQUEST (1<<3)
#define WLAN_CAPABILITY_PRIVACY   (1<<4)
#define WLAN_CAPABILITY_SHORT_PREAMBLE  (1<<5)
#define WLAN_CAPABILITY_PBCC    (1<<6)
#define WLAN_CAPABILITY_CHANNEL_AGILITY (1<<7)
#define WLAN_CAPABILITY_SPECTRUM_MGMT (1<<8)
#define WLAN_CAPABILITY_QOS   (1<<9)
#define WLAN_CAPABILITY_SHORT_SLOT_TIME (1<<10)
#define WLAN_CAPABILITY_APSD    (1<<11)
#define WLAN_CAPABILITY_DSSS_OFDM (1<<13)

/**
 * Parse the result of the passive scan of nl80211
 * @param msg pointer to netlink message
 */
static void
_parse_cmd_new_scan_result(struct nlmsghdr *msg) {
  static struct nla_policy bss_policy[NL80211_BSS_MAX + 1] = {
    [NL80211_BSS_TSF]             = { .type = NLA_U64 },
    [NL80211_BSS_FREQUENCY]       = { .type = NLA_U32 },
//    [NL80211_BSS_BSSID] = { },
    [NL80211_BSS_BEACON_INTERVAL] = { .type = NLA_U16 },
    [NL80211_BSS_CAPABILITY]      = { .type = NLA_U16 },
//    [NL80211_BSS_INFORMATION_ELEMENTS] = { },
    [NL80211_BSS_SIGNAL_MBM]      = { .type = NLA_U32 },
    [NL80211_BSS_SIGNAL_UNSPEC]   = { .type = NLA_U8 },
    [NL80211_BSS_STATUS]          = { .type = NLA_U32 },
    [NL80211_BSS_SEEN_MS_AGO]     = { .type = NLA_U32 },
//    [NL80211_BSS_BEACON_IES] = { },
  };

  struct nlattr *tb[NL80211_ATTR_MAX + 1];
  struct nlattr *bss[NL80211_BSS_MAX + 1];

  struct oonf_interface_data *if_data;
  struct oonf_layer2_network *net;
  struct netaddr mac;
  unsigned if_index;
  char if_name[IF_NAMESIZE];
#ifdef OONF_LOG_DEBUG_INFO
  struct netaddr_str buf;
#endif

  if (nlmsg_parse(msg, sizeof(struct genlmsghdr),
      tb, NL80211_ATTR_MAX, NULL) < 0) {
    OONF_WARN(LOG_NL80211, "Cannot parse netlink NL80211_CMD_NEW_SCAN_RESULT message");
    return;
  }

  if (!tb[NL80211_ATTR_BSS]) {
    OONF_WARN(LOG_NL80211, "bss info missing!\n");
    return;
  }
  if (nla_parse_nested(bss, NL80211_BSS_MAX,
           tb[NL80211_ATTR_BSS],
           bss_policy)) {
    OONF_WARN(LOG_NL80211, "failed to parse nested attributes!\n");
    return;
  }

  if (!bss[NL80211_BSS_BSSID]) {
    OONF_WARN(LOG_NL80211, "No BSSID found");
    return;
  }

  if (!bss[NL80211_BSS_STATUS]) {
    /* ignore different networks for the moment */
    return;
  }
  netaddr_from_binary(&mac, nla_data(bss[NL80211_BSS_BSSID]), 6, AF_MAC48);
  if_index = nla_get_u32(tb[NL80211_ATTR_IFINDEX]);

  if (if_indextoname(if_index, if_name) == NULL) {
    return;
  }

  if_data = oonf_interface_get_data(if_name, NULL);
  if (if_data == NULL || netaddr_get_address_family(&if_data->mac) == AF_UNSPEC) {
    return;
  }

  net = oonf_layer2_add_network(&if_data->mac, if_index,
      _config.interval + _config.interval / 4);
  if (net == NULL) {
    OONF_WARN(LOG_NL80211, "Not enough memory for new layer2 network");
    return;
  }

  OONF_DEBUG(LOG_NL80211, "Add network %s", netaddr_to_string(&buf, &if_data->mac));
#if 0
  if (bss[NL80211_BSS_STATUS]) {
    switch (nla_get_u32(bss[NL80211_BSS_STATUS])) {
    case NL80211_BSS_STATUS_AUTHENTICATED:
      printf(" -- authenticated");
      break;
    case NL80211_BSS_STATUS_ASSOCIATED:
      printf(" -- associated");
      break;
    case NL80211_BSS_STATUS_IBSS_JOINED:
      printf(" -- joined");
      break;
    default:
      printf(" -- unknown status: %d",
        nla_get_u32(bss[NL80211_BSS_STATUS]));
      break;
    }
  }
  printf("\n");

  if (bss[NL80211_BSS_TSF]) {
    unsigned long long tsf;
    tsf = (unsigned long long)nla_get_u64(bss[NL80211_BSS_TSF]);
    printf("\tTSF: %llu usec (%llud, %.2lld:%.2llu:%.2llu)\n",
      tsf, tsf/1000/1000/60/60/24, (tsf/1000/1000/60/60) % 24,
      (tsf/1000/1000/60) % 60, (tsf/1000/1000) % 60);
  }
#endif

  if (bss[NL80211_BSS_FREQUENCY]) {
    oonf_layer2_network_set_frequency(net,
        nla_get_u32(bss[NL80211_BSS_FREQUENCY]) * 1000000ull);
  }
#if 0
  if (bss[NL80211_BSS_BEACON_INTERVAL])
    printf("\tbeacon interval: %d\n",
      nla_get_u16(bss[NL80211_BSS_BEACON_INTERVAL]));
  if (bss[NL80211_BSS_CAPABILITY]) {
    __u16 capa = nla_get_u16(bss[NL80211_BSS_CAPABILITY]);
    printf("\tcapability:");
    if (capa & WLAN_CAPABILITY_ESS)
      printf(" ESS");
    if (capa & WLAN_CAPABILITY_IBSS)
      printf(" IBSS");
    if (capa & WLAN_CAPABILITY_PRIVACY)
      printf(" Privacy");
    if (capa & WLAN_CAPABILITY_SHORT_PREAMBLE)
      printf(" ShortPreamble");
    if (capa & WLAN_CAPABILITY_PBCC)
      printf(" PBCC");
    if (capa & WLAN_CAPABILITY_CHANNEL_AGILITY)
      printf(" ChannelAgility");
    if (capa & WLAN_CAPABILITY_SPECTRUM_MGMT)
      printf(" SpectrumMgmt");
    if (capa & WLAN_CAPABILITY_QOS)
      printf(" QoS");
    if (capa & WLAN_CAPABILITY_SHORT_SLOT_TIME)
      printf(" ShortSlotTime");
    if (capa & WLAN_CAPABILITY_APSD)
      printf(" APSD");
    if (capa & WLAN_CAPABILITY_DSSS_OFDM)
      printf(" DSSS-OFDM");
    printf(" (0x%.4x)\n", capa);
  }
  if (bss[NL80211_BSS_SIGNAL_MBM]) {
    int s = nla_get_u32(bss[NL80211_BSS_SIGNAL_MBM]);
    printf("\tsignal: %d.%.2d dBm\n", s/100, s%100);
  }
  if (bss[NL80211_BSS_SIGNAL_UNSPEC]) {
    unsigned char s = nla_get_u8(bss[NL80211_BSS_SIGNAL_UNSPEC]);
    printf("\tsignal: %d/100\n", s);
  }
#endif
  if (bss[NL80211_BSS_SEEN_MS_AGO]) {
    oonf_layer2_network_set_last_seen(net,
        nla_get_u32(bss[NL80211_BSS_SEEN_MS_AGO]));
  }
  if (bss[NL80211_BSS_INFORMATION_ELEMENTS] != NULL ||
      bss[NL80211_BSS_BEACON_IES] != NULL) {
    int len,i;
    uint8_t *data;
    uint8_t *rate1, *rate2;
    uint8_t rate1_count, rate2_count;
    uint64_t *rate;

    rate1 = rate2 = NULL;

    if (bss[NL80211_BSS_INFORMATION_ELEMENTS]) {
      len = nla_len(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
      data = nla_data(bss[NL80211_BSS_INFORMATION_ELEMENTS]);
    }
    else {
      len = nla_len(bss[NL80211_BSS_BEACON_IES]);
      data = nla_data(bss[NL80211_BSS_BEACON_IES]);
    }

    /* collect pointers to data-rates */
    rate1_count = 0;
    rate2_count = 0;
    while (len > 0) {
      if (data[0] == 0) {
        /* SSID */
        char ssid[33];

        memset(ssid, 0, sizeof(ssid));
        memcpy(ssid, &data[2], data[1]);
        oonf_layer2_network_set_ssid(net, ssid);
      }
      if (data[0] == 1) {
        /* supported rates */
        rate1 = &data[2];
        rate1_count = data[1];
      }
      else if (data[0] == 50) {
        /* extended supported rates */
        rate2 = &data[2];
        rate2_count = data[1];
      }
      len -= data[1] + 2;
      data += data[1] + 2;
    }

    if (rate1_count + rate2_count > 0) {
      rate = calloc(rate1_count + rate2_count, sizeof(uint64_t));
      if (rate) {
        len = 0;
        for (i=0; i<rate1_count; i++) {
          rate[len++] = (uint64_t)(rate1[i] & 0x7f) << 19;
        }
        for (i=0; i<rate2_count; i++) {
          rate[len++] = (uint64_t)(rate2[i] & 0x7f) << 19;
        }

        oonf_layer2_network_set_supported_rates(net, rate, rate1_count + rate2_count);
        free(rate);
      }
    }
  }

  oonf_layer2_network_commit(net);
  return;
}

/**
 * Parse an incoming netlink message from the kernel
 * @param hdr pointer to netlink message
 */
static void
_cb_nl_message(struct nlmsghdr *hdr) {
  struct genlmsghdr *gen_hdr;

  gen_hdr = NLMSG_DATA(hdr);
  if (hdr->nlmsg_type == GENL_ID_CTRL && gen_hdr->cmd == CTRL_CMD_NEWFAMILY) {
    _parse_cmd_newfamily(hdr);
    return;
  }

  if (hdr->nlmsg_type == _nl80211_id) {
    if (gen_hdr->cmd == NL80211_CMD_NEW_STATION) {
      _parse_cmd_new_station(hdr);
      return;
    }
    if (gen_hdr->cmd == NL80211_CMD_DEL_STATION) {
      _parse_cmd_del_station(hdr);
      return;
    }
    if (gen_hdr->cmd == NL80211_CMD_NEW_SCAN_RESULTS) {
      _parse_cmd_new_scan_result(hdr);
      return;
    }
  }

  OONF_INFO(LOG_NL80211, "Unhandled incoming netlink message type %u cmd %u\n",
      hdr->nlmsg_type, gen_hdr->cmd);
}

/**
 * Request the list of generic netlink families from the kernel
 */
static void
_send_genl_getfamily(void) {
  struct genlmsghdr *hdr;

  memset(_msgbuf, 0, UIO_MAXIOV);

  /* generic netlink initialization */
  hdr = NLMSG_DATA(_msgbuf);
  _msgbuf->nlmsg_len = NLMSG_LENGTH(sizeof(*hdr));
  _msgbuf->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

  /* request nl80211 identifier */
  _msgbuf->nlmsg_type = GENL_ID_CTRL;

  hdr->cmd = CTRL_CMD_GETFAMILY;
  hdr->version = 1;

  os_system_netlink_send(&_netlink_handler, _msgbuf);
}

/**
 * Request a station dump from nl80211
 * @param if_idx interface index to be dumped
 */
static void
_send_nl80211_get_station_dump(int if_idx) {
  struct genlmsghdr *hdr;

  memset(_msgbuf, 0, UIO_MAXIOV);

  /* generic netlink initialization */
  hdr = NLMSG_DATA(_msgbuf);
  _msgbuf->nlmsg_len = NLMSG_LENGTH(sizeof(*hdr));
  _msgbuf->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

  /* get nl80211 station dump */
  _msgbuf->nlmsg_type = _nl80211_id;
  hdr->cmd = NL80211_CMD_GET_STATION;

  /* add interface index to the request */
  os_system_netlink_addreq(_msgbuf, NL80211_ATTR_IFINDEX, &if_idx, sizeof(if_idx));

  os_system_netlink_send(&_netlink_handler, _msgbuf);
}

/**
 * Request a passive scan dump from nl80211
 * @param if_idx interface index to be dumped
 */
static void
_send_nl80211_get_scan_dump(int if_idx) {
  struct genlmsghdr *hdr;

  memset(_msgbuf, 0, UIO_MAXIOV);

  /* generic netlink initialization */
  hdr = NLMSG_DATA(_msgbuf);
  _msgbuf->nlmsg_len = NLMSG_LENGTH(sizeof(*hdr));
  _msgbuf->nlmsg_flags = NLM_F_REQUEST | NLM_F_DUMP;

  /* get nl80211 station dump */
  _msgbuf->nlmsg_type = _nl80211_id;
  hdr->cmd = NL80211_CMD_GET_SCAN;

  /* add interface index to the request */
  os_system_netlink_addreq(_msgbuf, NL80211_ATTR_IFINDEX, &if_idx, sizeof(if_idx));

  os_system_netlink_send(&_netlink_handler, _msgbuf);
}

/**
 * Transmit the next netlink command to nl80211
 * @param ptr unused
 */
static void
_cb_transmission_event(void *ptr __attribute__((unused))) {
  struct oonf_interface *interf;

  if (_last_queried_if[0] == 0) {
    /* get first interface */
    interf = avl_first_element(&oonf_interface_tree, interf, _node);
  }
  else {
    /* get next interface */
    interf = avl_find_ge_element(&oonf_interface_tree, _last_queried_if, interf, _node);

    if (interf != NULL && strcmp(_last_queried_if, interf->data.name) == 0) {
      interf = avl_next_element_safe(&oonf_interface_tree, interf, _node);
    }
  }

  if (!interf && _next_query_type < QUERY_COUNT-1) {
    /* begin next query type */
    _next_query_type++;
    interf = avl_first_element(&oonf_interface_tree, interf, _node);
  }

  if (!interf) {
    /* nothing to do anymore */
    memset(_last_queried_if, 0, sizeof(_last_queried_if));
    _next_query_type = QUERY_FIRST;
    return;
  }
  else {
    strscpy(_last_queried_if, interf->data.name, sizeof(_last_queried_if));
  }

  OONF_DEBUG(LOG_NL80211, "Send Query %d to NL80211 interface %s",
      _next_query_type, interf->data.name);
  if (_next_query_type == QUERY_STATION_DUMP) {
    _send_nl80211_get_station_dump(interf->data.index);
  }
  else if (_next_query_type == QUERY_SCAN_DUMP){
    _send_nl80211_get_scan_dump(interf->data.index);
  }
}

static void
_cb_nl_error(uint32_t seq, int error) {
  OONF_DEBUG(LOG_NL80211, "%u: Received error %d", seq, error);
  _cb_transmission_event(NULL);
}

static void
_cb_nl_timeout(void) {
  OONF_DEBUG(LOG_NL80211, "Received timeout");
  _cb_transmission_event(NULL);
}

static void
_cb_nl_done(uint32_t seq) {
  OONF_DEBUG(LOG_NL80211, "%u: Received done", seq);
  _cb_transmission_event(NULL);
}

/**
 * Update configuration of nl80211-listener plugin
 */
static void
_cb_config_changed(void) {
  if (cfg_schema_tobin(&_config, _nl80211_section.post,
      _nl80211_entries, ARRAYSIZE(_nl80211_entries))) {
    OONF_WARN(LOG_NL80211, "Could not convert %s config to bin",
        OONF_PLUGIN_GET_NAME());
    return;
  }

  /* half of them station dumps, half of them passive scans */
  oonf_timer_start(&_transmission_timer, _config.interval);
}

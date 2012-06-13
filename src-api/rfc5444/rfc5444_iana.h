
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

#ifndef RFC5444_IANA_H_
#define RFC5444_IANA_H_

#include "common/common_types.h"
#include "common/netaddr.h"

/*
 * IANA registered IP/UDP-port number
 * and multicast groups for MANET (RFC 5498)
 */

enum rfc5444_iana {
  RFC5444_MANET_IPPROTO  = 138,
  RFC5444_MANET_UDP_PORT = 269,
};

EXPORT extern const struct netaddr RFC5444_MANET_MULTICAST_V4;
EXPORT extern const struct netaddr RFC5444_MANET_MULTICAST_V6;

/*
 * text variants of the constants above for defaults in
 * configuration sections
 */
EXPORT extern const char RFC5444_MANET_IPPROTO_TXT[];
EXPORT extern const char RFC5444_MANET_UDP_PORT_TXT[];
EXPORT extern const char RFC5444_MANET_MULTICAST_V4_TXT[];
EXPORT extern const char RFC5444_MANET_MULTICAST_V6_TXT[];

/*
 * this is a list of all globally defined IANA
 * message types
 */

enum rfc5444_msgtype_iana {
  /* RFC 6130 (NHDP) */
  RFC5444_MSGTYPE_HELLO = 0,
};

/*
 * this is a list of all globally defined IANA
 * packet TLVs and their allocated values
 */

enum rfc5444_pkttlvs_iana {
  /* RFC 6622 (rfc5444-sec) */
  RFC5444_PKTTLV_ICV       = 5,
  RFC5444_PKTTLV_TIMESTAMP = 6,
};

/*
 * this is a list of all globally defined IANA
 * message TLVs and their allocated values
 */

enum rfc5444_msgtlvs_iana {
  /* RFC 5497 (timetlv) */
  RFC5444_MSGTLV_VALIDITY_TIME = 0,
  RFC5444_MSGTLV_INTERVAL_TIME = 1,

  /* RFC 6622 (rfc5444-sec) */
  RFC5444_MSGTLV_ICV           = 5,
  RFC5444_MSGTLV_TIMESTAMP     = 6,
};

/*
 * this is a list of all globally defined IANA
 * address TLVs and their allocated values
 */

enum rfc5444_addrtlv_iana {
  /* RFC 5497 (timetlv) */
  RFC5444_ADDRTLV_VALIDITY_TIME = 0,
  RFC5444_ADDRTLV_INTERVAL_TIME = 1,

  /* RFC 6130 (NHDP) */
  RFC5444_ADDRTLV_LOCAL_IF      = 2,
  RFC5444_ADDRTLV_LINK_STATUS   = 3,
  RFC5444_ADDRTLV_OTHER_NEIGHB  = 4,

  /* RFC 6622 (rfc5444-sec) */
  RFC5444_ADDRTLV_ICV           = 5,
  RFC5444_ADDRTLV_TIMESTAMP     = 6,
};

/* values for LOCAL_IF address TLV */
static const uint8_t RFC5444_LOCALIF_THIS_IF       = 0;
static const uint8_t RFC5444_LOCALIF_OTHER_IF      = 1;

/* values for LINK_STATUS address TLV */
static const uint8_t RFC5444_LINKSTATUS_LOST       = 0;
static const uint8_t RFC5444_LINKSTATUS_SYMMETRIC  = 1;
static const uint8_t RFC5444_LINKSTATUS_HEARD      = 2;

/* values for OTHER_NEIGHB address TLV */
static const uint8_t RFC5444_OTHERNEIGHB_LOST      = 0;
static const uint8_t RFC5444_OTHERNEIGHB_SYMMETRIC = 1;

#endif /* RFC5444_IANA_H_ */

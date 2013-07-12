
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
#include <assert.h>
#include <string.h>
#include <stdio.h>

#include "common/common_types.h"
#include "test_rfc5444_interop.h"

static uint8_t _binary[] = {
    0x0c, 0x00, 0x0d, 0x00, 0x02, 0x01, 0x00, 0x01, 0x03, 0x00, 0x08, 0x00,
    0x02, 0x01, 0x00, 0x02, 0xf3, 0x00, 0x0e, 0x0a, 0x00, 0x00, 0x01, 0xff,
    0x01, 0x30, 0x39, 0x00, 0x00 };

static uint8_t _originator[] = { 10, 0, 0, 1 };

static struct test_tlv _msgtlvs[] = {
  { .type = 1 },
};

static struct test_message _msgs[] = {
  {
    .type = 1,
    .addrlen = 4,

    .tlv_count = ARRAYSIZE(_msgtlvs),
    .tlvs = _msgtlvs,
  },
  {
    .type = 2,
    .addrlen = 4,
    .flags = 240,

    .has_originator = true,
    .originator = _originator,
    .has_hopcount = true,
    .hopcount = 1,
    .has_hoplimit = true,
    .hoplimit = 255,
    .has_seqno = true,
    .seqno = 12345,
  },
};

static struct test_tlv _pkttlvs[] = {
  { .type = 1 },
};

static struct test_packet test13 = {
  .test = "Interop 2010 Test 13",
  .binary = _binary,
  .binlen = ARRAYSIZE(_binary),

  .version = 0,
  .flags = 0x0c,

  .has_seq = true,
  .seqno = 13,

  .tlv_count = ARRAYSIZE(_pkttlvs),
  .tlvs = _pkttlvs,

  .msg_count = ARRAYSIZE(_msgs),
  .msgs = _msgs,
};

ADD_TEST(test13)

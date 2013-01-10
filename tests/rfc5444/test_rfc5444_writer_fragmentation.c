/*
 * RFC 5444 handler library
 * Copyright (c) 2010 Henning Rogge <hrogge@googlemail.com>
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
 * Visit http://www.olsr.org/git for more information.
 *
 * If you find this software useful feel free to make a donation
 * to the project. For more information see the website or contact
 * the copyright holders.
 */

#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "rfc5444/rfc5444_context.h"
#include "rfc5444/rfc5444_writer.h"
#include "cunit/cunit.h"

#define MSG_TYPE 1

static void write_packet(struct rfc5444_writer *,
    struct rfc5444_writer_interface *, void *, size_t);
static void addAddresses(struct rfc5444_writer *wr,
    struct rfc5444_writer_content_provider *provider);

static uint8_t msg_buffer[128];
static uint8_t msg_addrtlvs[1000];

static struct rfc5444_writer writer = {
  .msg_buffer = msg_buffer,
  .msg_size = sizeof(msg_buffer),
  .addrtlv_buffer = msg_addrtlvs,
  .addrtlv_size = sizeof(msg_addrtlvs),
};

static struct rfc5444_writer_content_provider cpr = {
  .msg_type = MSG_TYPE,
  .addAddresses = addAddresses,
};

static struct rfc5444_writer_addrtlv_block addrtlvs[] = {
  { .type = 3 },
};

static uint8_t packet_buffer_if1[128];
static struct rfc5444_writer_interface small_if = {
  .packet_buffer = packet_buffer_if1,
  .packet_size = sizeof(packet_buffer_if1),
  .sendPacket = write_packet,
};

static uint8_t packet_buffer_if2[256];
static struct rfc5444_writer_interface large_if = {
  .packet_buffer = packet_buffer_if2,
  .packet_size = sizeof(packet_buffer_if2),
  .sendPacket = write_packet,
};

static int tlvcount, fragments, packets[2];

static uint8_t tlv_value_buffer[256];
static uint8_t *tlv_value;
static size_t tlv_value_size;

static void addMessageHeader(struct rfc5444_writer *wr, struct rfc5444_writer_message *msg) {
  rfc5444_writer_set_msg_header(wr, msg, false, false, false, false);
}

static void finishMessageHeader(struct rfc5444_writer *wr  __attribute__ ((unused)),
    struct rfc5444_writer_message *msg __attribute__ ((unused)),
    struct rfc5444_writer_address *first_addr __attribute__ ((unused)),
    struct rfc5444_writer_address *last_addr __attribute__ ((unused)),
    bool not_fragmented __attribute__ ((unused))) {
  fragments++;
}

static void addAddresses(struct rfc5444_writer *wr,
    struct rfc5444_writer_content_provider *provider) {
  uint8_t ip[4] = { 10, 0, 0, 0 };
  struct rfc5444_writer_address *addr;
  int i;

  for (i=0; i<tlvcount; i++) {
    ip[3] = i+1;

    if (tlv_value) {
      tlv_value[tlv_value_size-1] = (uint8_t)(i & 255);
    }

    addr = rfc5444_writer_add_address(wr, provider->creator, ip, 32, false);
    rfc5444_writer_add_addrtlv(wr, addr, addrtlvs[0]._tlvtype, tlv_value, tlv_value_size, false);

    if (tlv_value) {
      tlv_value[tlv_value_size-1] = (tlv_value_size-1) & 255;
    }
  }
}

static void write_packet(struct rfc5444_writer *w __attribute__ ((unused)),
    struct rfc5444_writer_interface *iface,
    void *buffer, size_t length) {
  size_t i, j;
  uint8_t *buf = buffer;

  if (iface == &small_if) {
    printf("Interface 1:\n");
    packets[0]++;
  }
  else {
    printf("Interface 2:\n");
    packets[1]++;
  }

  for (j=0; j<length; j+=32) {
    printf("%04zx:", j);

    for (i=j; i<length && i < j+31; i++) {
      printf("%s%02x", ((i&3) == 0) ? " " : "", (int)(buf[i]));
    }
    printf("\n");
  }
  printf("\n");
}

static void clear_elements(void) {
  fragments = 0;
  tlv_value = NULL;
  tlv_value_size = 0;
  packets[0] = packets[1] = 0;
}

static void test_frag_80_1(void) {
  START_TEST();

  tlvcount = 1;
  tlv_value = tlv_value_buffer;
  tlv_value_size = 80;

  CHECK_TRUE(0 == rfc5444_writer_create_message_allif(&writer, 1), "Parser should return 0");
  rfc5444_writer_flush(&writer, &small_if, false);
  rfc5444_writer_flush(&writer, &large_if, false);

  CHECK_TRUE(fragments == 1, "bad number of fragments: %d\n", fragments);
  CHECK_TRUE(packets[0] == 1, "bad number of packets on if 1: %d\n", packets[0]);
  CHECK_TRUE(packets[1] == 1, "bad number of packets on if 2: %d\n", packets[1]);

  END_TEST();
}

static void test_frag_80_2(void) {
  START_TEST();

  tlvcount = 2;
  tlv_value = tlv_value_buffer;
  tlv_value_size = 80;

  CHECK_TRUE(0 == rfc5444_writer_create_message_allif(&writer, 1), "Parser should return 0");
  rfc5444_writer_flush(&writer, &small_if, false);
  rfc5444_writer_flush(&writer, &large_if, false);

  CHECK_TRUE(fragments == 2, "bad number of fragments: %d\n", fragments);
  CHECK_TRUE(packets[0] == 2, "bad number of packets on if 1: %d\n", packets[0]);
  CHECK_TRUE(packets[1] == 1, "bad number of packets on if 2: %d\n", packets[1]);

  END_TEST();
}

static void test_frag_80_3(void) {
  START_TEST();

  tlvcount = 3;
  tlv_value = tlv_value_buffer;
  tlv_value_size = 80;

  CHECK_TRUE(0 == rfc5444_writer_create_message_allif(&writer, 1), "Parser should return 0");
  rfc5444_writer_flush(&writer, &small_if, false);
  rfc5444_writer_flush(&writer, &large_if, false);

  CHECK_TRUE(fragments == 3, "bad number of fragments: %d\n", fragments);
  CHECK_TRUE(packets[0] == 3, "bad number of packets on if 1: %d\n", packets[0]);
  CHECK_TRUE(packets[1] == 2, "bad number of packets on if 2: %d\n", packets[1]);

  END_TEST();
}

static void test_frag_50_3(void) {
  START_TEST();

  tlvcount = 3;
  tlv_value = tlv_value_buffer;
  tlv_value_size = 50;

  CHECK_TRUE(0 == rfc5444_writer_create_message_allif(&writer, 1), "Parser should return 0");
  rfc5444_writer_flush(&writer, &small_if, false);
  rfc5444_writer_flush(&writer, &large_if, false);

  CHECK_TRUE(fragments == 2, "bad number of fragments: %d\n", fragments);
  CHECK_TRUE(packets[0] == 2, "bad number of packets on if 1: %d\n", packets[0]);
  CHECK_TRUE(packets[1] == 1, "bad number of packets on if 2: %d\n", packets[1]);

  END_TEST();
}

static void test_frag_150_3(void) {
  START_TEST();

  tlvcount = 3;
  tlv_value = tlv_value_buffer;
  tlv_value_size = 150;

  CHECK_TRUE(0 != rfc5444_writer_create_message_allif(&writer, 1), "Parser should return -1");

  CHECK_TRUE(fragments == 0, "bad number of fragments: %d\n", fragments);
  CHECK_TRUE(packets[0] == 0, "bad number of packets on if 1: %d\n", packets[0]);
  CHECK_TRUE(packets[1] == 0, "bad number of packets on if 2: %d\n", packets[1]);

  END_TEST();
}

int main(int argc __attribute__ ((unused)), char **argv __attribute__ ((unused))) {
  struct rfc5444_writer_message *msg;
  size_t i;

  for (i=0; i<sizeof(tlv_value_buffer); i++) {
    tlv_value_buffer[i] = i;
  }

  rfc5444_writer_init(&writer);

  rfc5444_writer_register_interface(&writer, &small_if);
  rfc5444_writer_register_interface(&writer, &large_if);

  msg = rfc5444_writer_register_message(&writer, MSG_TYPE, false, 4);
  msg->addMessageHeader = addMessageHeader;
  msg->finishMessageHeader = finishMessageHeader;

  rfc5444_writer_register_msgcontentprovider(&writer, &cpr, addrtlvs, ARRAYSIZE(addrtlvs));

  BEGIN_TESTING(clear_elements);

  test_frag_80_1();
  test_frag_80_2();
  test_frag_80_3();
  test_frag_50_3();
  test_frag_150_3();

  rfc5444_writer_cleanup(&writer);

  return FINISH_TESTING();
}

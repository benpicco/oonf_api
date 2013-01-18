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

#include "common/autobuf.h"
#include "rfc5444/rfc5444_context.h"
#include "rfc5444/rfc5444_print.h"
#include "rfc5444/rfc5444_writer.h"
#include "cunit/cunit.h"

uint8_t result[] = {
    0x00, 0x01, 0x03, 0x00, 0x28, 0x00, 0x00, 0x04,
    0x80, 0x01, 0x0a, 0x01, 0x00, 0x65, 0x01, 0x00,
    0x66, 0x01, 0x00, 0x67, 0x0b, 0x0b, 0x0b, 0x00,
    0x10, 0x02, 0x50, 0x01, 0x01, 0x00, 0x03, 0x50,
    0x00, 0x01, 0x01, 0x03, 0x30, 0x02, 0x03, 0x01,
    0x01
};

/*
0000: 00010300 28000004 80010a01 00650100 66010067 0b0b0b00 10025001 01000350
0020: 00010103 30020301 01

        ,------------------
        |  PACKET
        |------------------
        | * Packet version:    0
        | * Packet flags:      0x0
        |    ,-------------------
        |    |  MESSAGE
        |    |-------------------
        |    | * Message type:       1
        |    | * Message flags:      0x00
        |    | * Address length:     4
        |    |    ,-------------------
        |    |    |  Address: 10.1.0.101/32
        |    |    |    | - TLV
        |    |    |    |     Flags = 0x50
        |    |    |    |     Type = 3
        |    |    |    |     Value length: 1
        |    |    |    |       0000: 01
        |    |    `-------------------
        |    |    ,-------------------
        |    |    |  Address: 10.1.0.102/32
        |    |    |    | - TLV
        |    |    |    |     Flags = 0x50
        |    |    |    |     Type = 2
        |    |    |    |     Value length: 1
        |    |    |    |       0000: 00
        |    |    `-------------------
        |    |    ,-------------------
        |    |    |  Address: 10.1.0.103/32
        |    |    |    | - TLV
        |    |    |    |     Flags = 0x30
        |    |    |    |     Type = 3
        |    |    |    |     Value length: 1
        |    |    |    |       0000: 01
        |    |    `-------------------
        |    |    ,-------------------
        |    |    |  Address: 10.11.11.11/32
        |    |    |    | - TLV
        |    |    |    |     Flags = 0x30
        |    |    |    |     Type = 3
        |    |    |    |     Value length: 1
        |    |    |    |       0000: 01
        |    |    `-------------------
        |    `-------------------
        `------------------
 */

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
    { .type = 2 },
    { .type = 3 },
};

static uint8_t packet_buffer_if[128];
static struct rfc5444_writer_interface out_if = {
  .packet_buffer = packet_buffer_if,
  .packet_size = sizeof(packet_buffer_if),
  .sendPacket = write_packet,
};

static void addMessageHeader(struct rfc5444_writer *wr, struct rfc5444_writer_message *msg) {
  rfc5444_writer_set_msg_header(wr, msg, false, false, false, false);
}

static void addAddresses(struct rfc5444_writer *wr,
    struct rfc5444_writer_content_provider *provider) {
  uint8_t ip[4] = { 10,1, 0, 101 };
  struct rfc5444_writer_address *addr;
  char value0 =  0, value1 = 1;

  ip[0] = 10; ip[1] = 1; ip[2] = 0; ip[3] = 101;
  addr = rfc5444_writer_add_address(wr, provider->creator, ip, 32, false);
  rfc5444_writer_add_addrtlv(wr, addr, addrtlvs[1]._tlvtype, &value1, 1, false);

  ip[0] = 10; ip[1] = 1; ip[2] = 0; ip[3] = 102;
  addr = rfc5444_writer_add_address(wr, provider->creator, ip, 32, false);
  rfc5444_writer_add_addrtlv(wr, addr, addrtlvs[0]._tlvtype, &value0, 1, false);

  ip[0] = 10; ip[1] = 1; ip[2] = 0; ip[3] = 103;
  addr = rfc5444_writer_add_address(wr, provider->creator, ip, 32, false);
  rfc5444_writer_add_addrtlv(wr, addr, addrtlvs[1]._tlvtype, &value1, 1, false);

  ip[0] = 10; ip[1] = 11; ip[2] = 11; ip[3] = 11;
  addr = rfc5444_writer_add_address(wr, provider->creator, ip, 32, false);
  rfc5444_writer_add_addrtlv(wr, addr, addrtlvs[1]._tlvtype, &value1, 1, false);
}

static void write_packet(struct rfc5444_writer *w __attribute__ ((unused)),
    struct rfc5444_writer_interface *iface __attribute__((unused)),
    void *buffer, size_t length) {
  size_t i, j;
  uint8_t *buf = buffer;
  struct autobuf out;

  for (j=0; j<length; j+=32) {
    printf("%04zx:", j);

    for (i=j; i<length && i < j+32; i++) {
      printf("%s%02x", ((i&3) == 0) ? " " : "", (int)(buf[i]));
    }
    printf("\n");
  }
  printf("\n");

  abuf_init(&out);
  rfc5444_print_direct(&out, buf, length);

  printf("%s\n", abuf_getptr(&out));
  abuf_free(&out);

  CHECK_TRUE(length == sizeof(result), "Result has wrong length: %zu != %zu", length, sizeof(result));
  if (length == sizeof(result)) {
    CHECK_TRUE(memcmp(result, buffer, sizeof(result)) == 0, "Result differs from pattern");
  }
}


static void clear_elements(void) {
}

static void test(void) {
  START_TEST();

  CHECK_TRUE(0 == rfc5444_writer_create_message_allif(&writer, 1), "Parser should return 0");
  rfc5444_writer_flush(&writer, &out_if, false);

  END_TEST();
}

int main(int argc __attribute__ ((unused)), char **argv __attribute__ ((unused))) {
  struct rfc5444_writer_message *msg;

  rfc5444_writer_init(&writer);

  rfc5444_writer_register_interface(&writer, &out_if);

  msg = rfc5444_writer_register_message(&writer, MSG_TYPE, false, 4);
  msg->addMessageHeader = addMessageHeader;

  rfc5444_writer_register_msgcontentprovider(&writer, &cpr, addrtlvs, ARRAYSIZE(addrtlvs));

  BEGIN_TESTING(clear_elements);

  test();

  rfc5444_writer_cleanup(&writer);

  return FINISH_TESTING();
}

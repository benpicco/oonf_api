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

#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>

#include "rfc5444/rfc5444_reader.h"
#include "rfc5444/rfc5444_api_config.h"

#define PRINT_CB 1

#if DISALLOW_CONSUMER_CONTEXT_DROP == 0
#include "cunit/cunit.h"

/*
 * consumer definition 1
 * TLV type 1
 * TLV type 2
 */
static struct rfc5444_reader_tlvblock_consumer_entry consumer_entries[] = {
  { .type = 1 },
  { .type = 2 }
};

/* rfc5444 test message */
static uint8_t testpacket[] = {
/* packet with tlvblock, but without sequence number */
    0x04,
/* tlvblock, tlv type 1, tlv type 2 */
    0, 4, 1, 0, 2, 0,

/* message type 1, addrlen 4 */
    1, 0x03, 0, 26,
/* tlvblock, tlv type 1, tlv type 2 */
    0, 4, 1, 0, 2, 0,

/* address block with 2 IPs without compression */
    2, 0, 10, 0, 0, 1, 10, 0, 0, 2,
/* tlvblock, tlv type 1, tlv type 2 */
    0, 4, 1, 0, 2, 0,

/* message type 2, addrlen 4 */
    2, 0x03, 0, 10,
/* tlvblock, tlv type 1, tlv type 2 */
    0, 4, 1, 0, 2, 0,
};

static struct rfc5444_reader context;
static struct rfc5444_reader_tlvblock_consumer packet_consumer[2];
static struct rfc5444_reader_tlvblock_consumer msg1_consumer[2];
static struct rfc5444_reader_tlvblock_consumer msg1_addr_consumer[2];
static struct rfc5444_reader_tlvblock_consumer msg2_consumer;

static enum rfc5444_result result_start_packet[2];
static enum rfc5444_result result_start_message[2];
static enum rfc5444_result result_start_address[2][2];
static enum rfc5444_result result_blockcb_packet[2];
static enum rfc5444_result result_blockcb_message[2];
static enum rfc5444_result result_blockcb_address[2][2];
static enum rfc5444_result result_end_packet[2];
static enum rfc5444_result result_end_message[2];
static enum rfc5444_result result_end_address[2][2];
static enum rfc5444_result result_tlv_packet[2][2];
static enum rfc5444_result result_tlv_message[2][2];
static enum rfc5444_result result_tlv_address[2][2][2];

static int callback_index;

static int idxcb_start_packet[2];
static int idxcb_start_message[2];
static int idxcb_start_address[2][2];
static int idxcb_end_packet[2];
static int idxcb_end_message[2];
static int idxcb_end_address[2][2];
static int idxcb_blocktlv_packet[2];
static int idxcb_blocktlv_message[2];
static int idxcb_blocktlv_address[2][2];
static int idxcb_tlv_packet[2][2];
static int idxcb_tlv_message[2][2];
static int idxcb_tlv_address[2][2][2];

static int idxcb_start_message2;
static int idxcb_end_message2;
static int idxcb_blocktlv_message2;

static bool gottlv_blocktlv_packet[2][2];
static bool gottlv_blocktlv_message[2][2];
static bool gottlv_blocktlv_address[2][2][2];

static bool droptlv_blocktlv_packet[2][2];
static bool droptlv_blocktlv_message[2][2];
static bool droptlv_blocktlv_address[2][2][2];

static enum rfc5444_result
cb_blocktlv_packet(struct rfc5444_reader_tlvblock_consumer *consumer,
      struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused))) {
  int oi = consumer->order - 1;

#ifdef PRINT_CB
  printf("%s: packet blocktlv (order %d): %d\n", __func__, consumer->order, callback_index);
#endif
  idxcb_blocktlv_packet[oi] = callback_index++;

  if ((gottlv_blocktlv_packet[oi][0] = consumer_entries[0].tlv != NULL)) {
    consumer_entries[0].drop = droptlv_blocktlv_packet[oi][0];
  }
  if ((gottlv_blocktlv_packet[oi][1] = consumer_entries[1].tlv != NULL)) {
    consumer_entries[1].drop = droptlv_blocktlv_packet[oi][1];
  }

  return result_blockcb_packet[oi];
}

static enum rfc5444_result
cb_blocktlv_message(struct rfc5444_reader_tlvblock_consumer *consumer,
      struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused))) {
  int oi = consumer->order - 1;

#ifdef PRINT_CB
  printf("%s: message blocktlv (order %d): %d\n", __func__, consumer->order, callback_index);
#endif
  idxcb_blocktlv_message[oi] = callback_index++;

  if ((gottlv_blocktlv_message[oi][0] = consumer_entries[0].tlv != NULL)) {
    consumer_entries[0].drop = droptlv_blocktlv_message[oi][0];
  }
  if ((gottlv_blocktlv_message[oi][1] = consumer_entries[1].tlv != NULL)) {
    consumer_entries[1].drop = droptlv_blocktlv_message[oi][1];
  }

  return result_blockcb_message[oi];
}

static enum rfc5444_result
cb_blocktlv_message2(struct rfc5444_reader_tlvblock_consumer *consumer __attribute__ ((unused)),
      struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused))) {
#ifdef PRINT_CB
  printf("%s: message 2 blocktlv: %d\n", __func__, callback_index);
#endif
  idxcb_blocktlv_message2 = callback_index++;
  return RFC5444_OKAY;
}

static enum rfc5444_result
cb_blocktlv_address(struct rfc5444_reader_tlvblock_consumer *consumer,
      struct rfc5444_reader_tlvblock_context *ctx) {
  uint8_t ai = ctx->addr[3] - 1;
  int oi = consumer->order - 1;

#ifdef PRINT_CB
  printf("%s: address %d blocktlv (order %d): %d\n", __func__, ai+1, consumer->order, callback_index);
#endif
  idxcb_blocktlv_address[oi][ai] = callback_index++;

  if ((gottlv_blocktlv_address[oi][ai][0] = consumer_entries[0].tlv != NULL)) {
    consumer_entries[0].drop = droptlv_blocktlv_address[oi][ai][0];
  }
  if ((gottlv_blocktlv_address[oi][ai][1] = consumer_entries[1].tlv != NULL)) {
    consumer_entries[1].drop = droptlv_blocktlv_address[oi][ai][1];
  }

  return result_blockcb_address[oi][ai];
}

static enum rfc5444_result
cb_start_packet(struct rfc5444_reader_tlvblock_consumer *consumer,
    struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused))) {
  int oi = consumer->order - 1;

#ifdef PRINT_CB
  printf("%s: packet start (order %d): %d\n", __func__, consumer->order, callback_index);
#endif
  idxcb_start_packet[oi] = callback_index++;
  return result_start_packet[oi];
}

static enum rfc5444_result
cb_start_message(struct rfc5444_reader_tlvblock_consumer *consumer,
    struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused))) {
  int oi = consumer->order - 1;

#ifdef PRINT_CB
  printf("%s: message start (order %d): %d\n", __func__, consumer->order, callback_index);
#endif
  idxcb_start_message[oi] = callback_index++;
  return result_start_message[oi];
}

static enum rfc5444_result
cb_start_addr(struct rfc5444_reader_tlvblock_consumer *consumer,
    struct rfc5444_reader_tlvblock_context *ctx) {
  int oi = consumer->order - 1;
  uint8_t ai = ctx->addr[3] - 1;

#ifdef PRINT_CB
  printf("%s: address %d start (order %d): %d\n", __func__, ai+1, consumer->order, callback_index);
#endif
  idxcb_start_address[oi][ai] = callback_index++;
  return result_start_address[oi][ai];
}

static enum rfc5444_result
cb_tlv_packet(struct rfc5444_reader_tlvblock_consumer *consumer,
    struct rfc5444_reader_tlvblock_entry *tlv,
    struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused))) {
  int oi = consumer->order - 1;
  int ti = tlv->type - 1;

#ifdef PRINT_CB
  printf("%s: packet tlv %d (order %d): %d\n", __func__, tlv->type, consumer->order, callback_index);
#endif
  idxcb_tlv_packet[oi][ti] = callback_index++;
  return result_tlv_packet[oi][ti];
}

static enum rfc5444_result
cb_tlv_message(struct rfc5444_reader_tlvblock_consumer *consumer,
    struct rfc5444_reader_tlvblock_entry *tlv,
    struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused))) {
  int oi = consumer->order - 1;
  int ti = tlv->type - 1;

#ifdef PRINT_CB
  printf("%s: message tlv %d (order %d): %d\n", __func__, tlv->type, consumer->order, callback_index);
#endif
  idxcb_tlv_message[oi][ti] = callback_index++;
  return result_tlv_message[oi][ti];
}

static enum rfc5444_result
cb_tlv_address(struct rfc5444_reader_tlvblock_consumer *consumer,
    struct rfc5444_reader_tlvblock_entry *tlv,
    struct rfc5444_reader_tlvblock_context *ctx) {
  int oi = consumer->order - 1;
  uint8_t ai = ctx->addr[3] - 1;
  int ti = tlv->type - 1;

#ifdef PRINT_CB
  printf("%s: message tlv %d (order %d): %d\n", __func__, tlv->type, consumer->order, callback_index);
#endif
  idxcb_tlv_address[oi][ai][ti] = callback_index++;
  return result_tlv_address[oi][ai][ti];
}

static enum rfc5444_result
cb_end_packet(struct rfc5444_reader_tlvblock_consumer *consumer,
    struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused)),
    bool dropped __attribute__ ((unused))) {
  int oi = consumer->order - 1;

#ifdef PRINT_CB
  printf("%s: packet end (order %d): %d\n", __func__, consumer->order, callback_index);
#endif
  idxcb_end_packet[oi] = callback_index++;
  return result_end_packet[oi];
}

static enum rfc5444_result
cb_end_message(struct rfc5444_reader_tlvblock_consumer *consumer,
    struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused)),
    bool dropped __attribute__ ((unused))) {
  int oi = consumer->order - 1;

#ifdef PRINT_CB
  printf("%s: message end (order %d): %d\n", __func__, consumer->order, callback_index);
#endif
  idxcb_end_message[oi] = callback_index++;
  return result_end_message[oi];
}

static enum rfc5444_result
cb_end_addr(struct rfc5444_reader_tlvblock_consumer *consumer,
    struct rfc5444_reader_tlvblock_context *ctx,
    bool dropped __attribute__ ((unused))) {
  int oi = consumer->order - 1;
  uint8_t ai = ctx->addr[3] - 1;

#ifdef PRINT_CB
  printf("%s: address %d end (order %d): %d\n", __func__, ai+1, consumer->order, callback_index);
#endif
  idxcb_end_address[oi][ai] = callback_index++;
  return result_end_address[oi][ai];
}

static enum rfc5444_result
cb_start_message2(struct rfc5444_reader_tlvblock_consumer *consumer __attribute__ ((unused)),
    struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused))) {
#ifdef PRINT_CB
  printf("%s: message 2 start: %d\n", __func__, callback_index);
#endif
  idxcb_start_message2 = callback_index++;
  return RFC5444_OKAY;
}

static enum rfc5444_result
cb_end_message2(struct rfc5444_reader_tlvblock_consumer *consumer __attribute__ ((unused)),
    struct rfc5444_reader_tlvblock_context *c __attribute__ ((unused)),
    bool dropped __attribute__ ((unused))) {
#ifdef PRINT_CB
  printf("%s: message 2 end: %d\n", __func__, callback_index);
#endif
  idxcb_end_message2 = callback_index++;
  return RFC5444_OKAY;
}


static void clear_elements(void) {
  int order, addr;

  callback_index = 0;

  for (order = 1; order <= 2; order ++) {
    idxcb_start_packet[order-1] = -1;
    idxcb_tlv_packet[order-1][0] = -1;
    idxcb_tlv_packet[order-1][1] = -1;
    idxcb_blocktlv_packet[order-1] = -1;
    idxcb_end_packet[order-1] = -1;

    idxcb_start_message[order-1] = -1;
    idxcb_tlv_message[order-1][0] = -1;
    idxcb_tlv_message[order-1][1] = -1;
    idxcb_blocktlv_message[order-1] = -1;
    idxcb_end_message[order-1] = -1;

    result_start_packet[order-1] = RFC5444_OKAY;
    result_end_packet[order-1] = RFC5444_OKAY;
    result_blockcb_packet[order-1] = RFC5444_OKAY;
    result_tlv_packet[order-1][0] = RFC5444_OKAY;
    result_tlv_packet[order-1][1] = RFC5444_OKAY;

    result_start_message[order-1] = RFC5444_OKAY;
    result_end_message[order-1] = RFC5444_OKAY;
    result_blockcb_message[order-1] = RFC5444_OKAY;
    result_tlv_message[order-1][0] = RFC5444_OKAY;
    result_tlv_message[order-1][1] = RFC5444_OKAY;

    gottlv_blocktlv_packet[order-1][0] = false;
    gottlv_blocktlv_packet[order-1][1] = false;
    gottlv_blocktlv_message[order-1][0] = false;
    gottlv_blocktlv_message[order-1][1] = false;

    for (addr = 1; addr <= 2; addr ++) {
      idxcb_start_address[order-1][addr-1] = -1;
      idxcb_tlv_address[order-1][addr-1][0] = -1;
      idxcb_tlv_address[order-1][addr-1][1] = -1;
      idxcb_blocktlv_address[order-1][addr-1] = -1;
      idxcb_end_address[order-1][addr-1] = -1;

      result_start_address[order-1][addr-1] = RFC5444_OKAY;
      result_end_address[order-1][addr-1] = RFC5444_OKAY;
      result_blockcb_address[order-1][addr-1] = RFC5444_OKAY;
      result_tlv_address[order-1][addr-1][0] = RFC5444_OKAY;
      result_tlv_address[order-1][addr-1][1] = RFC5444_OKAY;

      gottlv_blocktlv_address[order-1][addr-1][0] = false;
      gottlv_blocktlv_address[order-1][addr-1][1] = false;
    }
  }

  idxcb_start_message2 = -1;
  idxcb_blocktlv_message2 = -1;
  idxcb_end_message2 = -1;
}

static void run(void) {
  rfc5444_reader_handle_packet(&context, testpacket, sizeof(testpacket));
}

#define CHECK_CB_T(counter, index, text) { CHECK_TRUE(counter == index, text": %d != %d", counter, index); counter++; }
#define CHECK_CB_F(counter, index, text) CHECK_TRUE(-1 == index, text": %d != %d", -1, index);

static void test_result_okay(void) {
  int idx = 0;

  START_TEST();
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],        "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],        "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],        "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],        "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0],     "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1],     "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0],     "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1],     "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],        "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],        "packet tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][0],        "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],        "message tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][0],     "address 1, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][1],     "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0],     "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1],     "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_blockcb_pkt_result_droppacket(void) {
  int idx = 0;
  START_TEST();

  result_blockcb_packet[0] = RFC5444_DROP_PACKET;
  run();

  /* packet (order 1) received, everything else not */
  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_F (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_F (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_F (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_F (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_F (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_F (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_F (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE(!gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_start_pkt_result_droppacket(void) {
  int idx = 0;
  START_TEST();

  result_start_packet[0] = RFC5444_DROP_PACKET;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_F (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_F (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_F (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_F (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_F (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_F (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_F (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE(!gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE(!gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_end_pkt_result_droppacket(void) {
  int idx = 0;

  result_end_packet[0] = RFC5444_DROP_PACKET;
  START_TEST();
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_blockcb_msg_result_dropmsg(void) {
  int idx = 0;
  START_TEST();

  result_blockcb_message[0] = RFC5444_DROP_MESSAGE;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_start_msg_result_dropmsg(void) {
  int idx = 0;
  START_TEST();

  result_start_message[0] = RFC5444_DROP_MESSAGE;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_end_msg_result_dropmsg(void) {
  int idx = 0;
  START_TEST();

  result_end_message[0] = RFC5444_DROP_MESSAGE;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_blockcb_msg_result_droppkt(void) {
  int idx = 0;
  START_TEST();

  result_blockcb_message[0] = RFC5444_DROP_PACKET;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_F (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_F (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_F (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_start_msg_result_droppkt(void) {
  int idx = 0;
  START_TEST();

  result_start_message[0] = RFC5444_DROP_PACKET;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_F (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_F (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_F (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_end_msg_result_droppkt(void) {
  int idx = 0;
  START_TEST();

  result_end_message[0] = RFC5444_DROP_PACKET;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_F (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_F (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_F (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_blockcb_addr1_result_dropaddr(void) {
  int idx = 0;
  START_TEST();

  result_blockcb_address[0][0] = RFC5444_DROP_ADDRESS;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_start_addr1_result_dropaddr(void) {
  int idx = 0;
  START_TEST();

  result_start_address[0][0] = RFC5444_DROP_ADDRESS;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_end_addr1_result_dropaddr(void) {
  int idx = 0;
  START_TEST();

  result_end_address[0][0] = RFC5444_DROP_ADDRESS;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_blockcb_addr1_result_dropmsg(void) {
  int idx = 0;
  START_TEST();

  result_blockcb_address[0][0] = RFC5444_DROP_MESSAGE;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_start_addr1_result_dropmsg(void) {
  int idx = 0;
  START_TEST();

  result_start_address[0][0] = RFC5444_DROP_MESSAGE;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_end_addr1_result_dropmsg(void) {
  int idx = 0;
  START_TEST();

  result_end_address[0][0] = RFC5444_DROP_MESSAGE;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_blockcb_addr1_result_droppkt(void) {
  int idx = 0;
  START_TEST();

  result_blockcb_address[0][0] = RFC5444_DROP_PACKET;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_F (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_F (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_F (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_start_addr1_result_droppkt(void) {
  int idx = 0;
  START_TEST();

  result_start_address[0][0] = RFC5444_DROP_PACKET;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_F (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_F (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_F (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_end_addr1_result_droppkt(void) {
  int idx = 0;
  START_TEST();

  result_end_address[0][0] = RFC5444_DROP_PACKET;
  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_F (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_F (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_F (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_F (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_F (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_F (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_F (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_F (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_F (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_F (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_blockcb_pkt_result_droptlv1(void) {
  int idx = 0;
  START_TEST();

  result_blockcb_packet[0] = RFC5444_DROP_TLV;
  droptlv_blocktlv_packet[0][0] = true;

  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE(!gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_blockcb_msg_result_droptlv1(void) {
  int idx = 0;

  START_TEST();

  result_blockcb_message[0] = RFC5444_DROP_TLV;
  droptlv_blocktlv_message[0][0] = true;

  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_blockcb_addr1_result_droptlv1(void) {
  int idx = 0;
  START_TEST();

  result_blockcb_address[0][0] = RFC5444_DROP_TLV;
  droptlv_blocktlv_address[0][0][0] = true;

  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_tlvcb_pkt_result_droptlv1(void) {
  int idx = 0;
  START_TEST();

  result_tlv_packet[0][0] = RFC5444_DROP_TLV;

  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE(!gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE(!gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_tlvcb_msg_result_droptlv1(void) {
  int idx = 0;

  START_TEST();

  result_tlv_message[0][0] = RFC5444_DROP_TLV;

  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}

static void test_tlvcb_addr1_result_droptlv1(void) {
  int idx = 0;
  START_TEST();

  result_tlv_address[0][0][0] = RFC5444_DROP_TLV;

  run();

  CHECK_CB_T (idx, idxcb_start_packet    [0],       "start packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][0],    "tlv 1 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [0][1],    "tlv 2 packet       (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [0],       "blocktlv packet    (order 1)");
  CHECK_CB_T (idx, idxcb_start_packet    [1],       "start packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][0],    "tlv 1 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_packet      [1][1],    "tlv 2 packet       (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_packet [1],       "blocktlv packet    (order 2)");

  CHECK_CB_T (idx, idxcb_start_message   [0],       "start message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][0],    "tlv 1 message      (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_message     [0][1],    "tlv 2 message      (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[0],       "blocktlv message   (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][0],    "start address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][0], "tlv 1 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][0][1], "tlv 2 address 1    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][0],    "blocktlv address 1 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][0],    "end address 1      (order 1)");

  CHECK_CB_T (idx, idxcb_start_address   [0][1],    "start address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][0], "tlv 1 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_tlv_address     [0][1][1], "tlv 2 address 2    (order 1)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[0][1],    "blocktlv address 2 (order 1)");
  CHECK_CB_T (idx, idxcb_end_address     [0][1],    "end address 2      (order 1)");

  CHECK_CB_T (idx, idxcb_end_message     [0],       "end message        (order 1)");

  CHECK_CB_T (idx, idxcb_start_message   [1],       "start message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][0],    "tlv 1 message      (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_message     [1][1],    "tlv 2 message      (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_message[1],       "blocktlv message   (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][0],    "start address 1    (order 2)");
  CHECK_CB_F (idx, idxcb_tlv_address     [1][0][0], "tlv 1 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][0][1], "tlv 2 address 1    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][0],    "blocktlv address 1 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][0],    "end address 1      (order 2)");

  CHECK_CB_T (idx, idxcb_start_address   [1][1],    "start address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][0], "tlv 1 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_tlv_address     [1][1][1], "tlv 2 address 2    (order 2)");
  CHECK_CB_T (idx, idxcb_blocktlv_address[1][1],    "blocktlv address 2 (order 2)");
  CHECK_CB_T (idx, idxcb_end_address     [1][1],    "end address 2      (order 2)");

  CHECK_CB_T (idx, idxcb_end_message     [1],       "end message        (order 2)");

  CHECK_CB_T (idx, idxcb_start_message2,            "start message 2    (order 3)");
  CHECK_CB_T (idx, idxcb_blocktlv_message2,         "blocktlv message 2 (order 3)");
  CHECK_CB_T (idx, idxcb_end_message2,              "end message 2      (order 3)");

  CHECK_CB_T (idx, idxcb_end_packet      [1],       "end packet         (order 2)");
  CHECK_CB_T (idx, idxcb_end_packet      [0],       "end packet         (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [0][0],    "packet tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_packet [0][1],    "packet tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][0],    "message tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_message[0][1],    "message tlv 2 (order 1)");
  CHECK_TRUE(!gottlv_blocktlv_address[0][0][0], "address 1, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][0][1], "address 1, tlv 2 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][0], "address 2, tlv 1 (order 1)");
  CHECK_TRUE( gottlv_blocktlv_address[0][1][1], "address 2, tlv 2 (order 1)");

  CHECK_TRUE( gottlv_blocktlv_packet [1][0],    "packet tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_packet [1][1],    "packet tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][0],    "message tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_message[1][1],    "message tlv 2 (order 2)");
  CHECK_TRUE(!gottlv_blocktlv_address[1][0][0], "address 1, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][0][1], "address 1, tlv 2 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][0], "address 2, tlv 1 (order 2)");
  CHECK_TRUE( gottlv_blocktlv_address[1][1][1], "address 2, tlv 2 (order 2)");

  END_TEST();
}


#endif

int
main(int argc __attribute__ ((unused)), char **argv __attribute__ ((unused))) {
#if DISALLOW_CONSUMER_CONTEXT_DROP == 0
  int order;

  rfc5444_reader_init(&context);

  for (order = 1; order <= 2; order++) {
    rfc5444_reader_add_packet_consumer(&context, &packet_consumer[order-1], consumer_entries, ARRAYSIZE(consumer_entries), order);
    packet_consumer[order-1].start_callback = cb_start_packet;
    packet_consumer[order-1].tlv_callback = cb_tlv_packet;
    packet_consumer[order-1].end_callback = cb_end_packet;
    packet_consumer[order-1].block_callback = cb_blocktlv_packet;

    rfc5444_reader_add_message_consumer(&context, &msg1_consumer[order-1], consumer_entries, ARRAYSIZE(consumer_entries), 1, order);
    msg1_consumer[order-1].start_callback = cb_start_message;
    msg1_consumer[order-1].tlv_callback = cb_tlv_message;
    msg1_consumer[order-1].end_callback = cb_end_message;
    msg1_consumer[order-1].block_callback = cb_blocktlv_message;

    rfc5444_reader_add_address_consumer(&context, &msg1_addr_consumer[order-1], consumer_entries, ARRAYSIZE(consumer_entries), 1, order);
    msg1_addr_consumer[order-1].start_callback = cb_start_addr;
    msg1_addr_consumer[order-1].tlv_callback = cb_tlv_address;
    msg1_addr_consumer[order-1].end_callback = cb_end_addr;
    msg1_addr_consumer[order-1].block_callback = cb_blocktlv_address;
  }

  rfc5444_reader_add_message_consumer(&context, &msg2_consumer, consumer_entries, ARRAYSIZE(consumer_entries), 2, 3);
  msg2_consumer.start_callback = cb_start_message2;
  msg2_consumer.end_callback = cb_end_message2;
  msg2_consumer.block_callback = cb_blocktlv_message2;

  BEGIN_TESTING(clear_elements);

  test_result_okay();

  test_blockcb_pkt_result_droppacket();
  test_start_pkt_result_droppacket();
  test_end_pkt_result_droppacket();

  test_blockcb_msg_result_dropmsg();
  test_start_msg_result_dropmsg();
  test_end_msg_result_dropmsg();
  test_blockcb_msg_result_droppkt();
  test_start_msg_result_droppkt();
  test_end_msg_result_droppkt();

  test_blockcb_addr1_result_dropaddr();
  test_start_addr1_result_dropaddr();
  test_end_addr1_result_dropaddr();
  test_blockcb_addr1_result_dropmsg();
  test_start_addr1_result_dropmsg();
  test_end_addr1_result_dropmsg();
  test_blockcb_addr1_result_droppkt();
  test_start_addr1_result_droppkt();
  test_end_addr1_result_droppkt();

  test_blockcb_pkt_result_droptlv1();
  test_blockcb_msg_result_droptlv1();
  test_blockcb_addr1_result_droptlv1();

  test_tlvcb_pkt_result_droptlv1();
  test_tlvcb_msg_result_droptlv1();
  test_tlvcb_addr1_result_droptlv1();

  rfc5444_reader_cleanup(&context);

  return FINISH_TESTING();
#else
  return 0;
#endif
}

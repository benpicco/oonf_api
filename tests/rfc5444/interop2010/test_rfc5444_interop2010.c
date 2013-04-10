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
#include <string.h>
#include <stdio.h>

#include "common/common_types.h"
#include "common/avl.h"
#include "common/avl_comp.h"
#include "rfc5444/rfc5444_reader.h"
#include "test_rfc5444_interop.h"

#include "cunit/cunit.h"

static enum rfc5444_result _pkt_start_callback(
    struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _pkt_end_callback(
    struct rfc5444_reader_tlvblock_context *context, bool dropped);
static enum rfc5444_result _pkt_tlv_callback(
    struct rfc5444_reader_tlvblock_entry *,
    struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _msg_start_callback(
    struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _msg_end_callback(
    struct rfc5444_reader_tlvblock_context *context, bool dropped);
static enum rfc5444_result _msg_tlv_callback(
    struct rfc5444_reader_tlvblock_entry *,
    struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _addr_start_callback(
    struct rfc5444_reader_tlvblock_context *context);
static enum rfc5444_result _addr_end_callback(
    struct rfc5444_reader_tlvblock_context *context, bool dropped);
static enum rfc5444_result _addr_tlv_callback(
    struct rfc5444_reader_tlvblock_entry *,
    struct rfc5444_reader_tlvblock_context *context);

static struct rfc5444_reader_tlvblock_consumer _packet_consumer = {
  .start_callback = _pkt_start_callback,
  .end_callback = _pkt_end_callback,
  .tlv_callback = _pkt_tlv_callback,
};
static struct rfc5444_reader_tlvblock_consumer _msg_consumer = {
  .default_msg_consumer = true,
  .start_callback = _msg_start_callback,
  .end_callback = _msg_end_callback,
  .tlv_callback = _msg_tlv_callback,
};
static struct rfc5444_reader_tlvblock_consumer _addr_consumer = {
  .default_msg_consumer = true,
  .addrblock_consumer = true,
  .start_callback = _addr_start_callback,
  .end_callback = _addr_end_callback,
  .tlv_callback = _addr_tlv_callback,
};
static struct rfc5444_reader reader;

static struct test_packet *_packet;
static struct test_message *_current_msg;
static struct test_address *_current_addr;

static struct avl_tree _test_tree;

static const char *
tostring(char *buffer, uint8_t *ptr, size_t len) {
  size_t i;

  for (i=0; i<len && i<16; i++) {
    sprintf(&buffer[i*3], "%02x ", (int)ptr[i]);
  }
  if (len > 16) {
    sprintf(&buffer[16*3-1], "...");
  }
  return buffer;
}

static enum rfc5444_result
_pkt_start_callback(struct rfc5444_reader_tlvblock_context *context) {
  CHECK_TRUE(context->pkt_version == _packet->version,
      "Pkt-version was %x (should be %x)\n", context->pkt_version, _packet->version);
  CHECK_TRUE(context->pkt_flags == _packet->flags,
      "Pkt-flags was %x (should be %x)\n", context->pkt_flags, _packet->flags);

  CHECK_TRUE(context->has_pktseqno == _packet->has_seq,
      "Pkt has %s seqno (should have %s)\n",
      context->has_pktseqno ? "" : "no ",
      _packet->has_seq ? "one" : "none");

  if (context->has_pktseqno) {
    CHECK_TRUE(context->pkt_seqno == _packet->seqno,
        "Pkt-seqno is %u (should be %u)\n", context->pkt_seqno, _packet->seqno);
  }

  return RFC5444_OKAY;
}

static enum rfc5444_result
_pkt_end_callback(struct rfc5444_reader_tlvblock_context *context __attribute__((unused)),
    bool dropped __attribute__((unused))) {
  char buffer[80];
  struct test_message *msg;
  struct test_address *addr;

  size_t i,j,k;

  for (i=0; i<_packet->tlv_count; i++) {
    CHECK_TRUE(_packet->tlvs[i].okay, "Pkt-TLV %u (ext %u) was not found or different\n",
        _packet->tlvs[i].type, _packet->tlvs[i].exttype);
  }

  for (i=0; i<_packet->msg_count; i++) {
    msg = &_packet->msgs[i];

    CHECK_TRUE(msg->okay, "Message %u was not found\n",
        msg->type);

    if (msg->okay) {
      for (j=0; j<msg->tlv_count; j++) {
        CHECK_TRUE(msg->tlvs[j].okay, "Msg %u TLV %u (ext %u) was not found or different\n",
            msg->type, msg->tlvs[j].type, msg->tlvs[j].exttype);
      }

      for (j=0; j<msg->address_count; j++) {
        addr = &msg->addrs[j];

        CHECK_TRUE(addr->okay, "Msg %u Address %s not found\n",
            msg->type, tostring(buffer, addr->addr, msg->addrlen));

        if (addr->okay) {
          for (k=0; k<addr->tlv_count; k++) {
            CHECK_TRUE(addr->tlvs[k].okay, "Msg %u Addr %s TLV %u (ext %u) was not found or different\n",
                msg->type, tostring(buffer, addr->addr, msg->addrlen),
                addr->tlvs[k].type, addr->tlvs[k].exttype);
          }
        }
      }
    }
  }
  return RFC5444_OKAY;
}

static enum rfc5444_result
_pkt_tlv_callback(struct rfc5444_reader_tlvblock_entry *entry,
    struct rfc5444_reader_tlvblock_context *context __attribute__((unused))) {
  struct test_tlv *tlv;
  size_t i;

  for (i=0; i<_packet->tlv_count; i++) {
    tlv = &_packet->tlvs[i];

    if (tlv->type == entry->type && tlv->exttype == entry->type_ext) {
      if (tlv->length == entry->length
        && memcmp(tlv->value, entry->single_value, tlv->length) == 0) {
        tlv->okay = true;
      }
      else {
        CHECK_TRUE(false, "Pkt TLV %u (ext %u) has wrong value\n",
            entry->type, entry->type_ext);
      }
      return RFC5444_OKAY;
    }
  }

  CHECK_TRUE(false, "Pkt TLV %u (ext %u) unknown", entry->type, entry->type_ext);
  return RFC5444_OKAY;
}

static enum rfc5444_result
_msg_start_callback(struct rfc5444_reader_tlvblock_context *context) {
  struct test_message *msg;
  char buf1[80], buf2[80];
  size_t i;

  msg = NULL;
  for (i=0; i<_packet->msg_count; i++) {
    if (_packet->msgs[i].type == context->msg_type) {
      msg = &_packet->msgs[i];
      break;
    }
  }

  CHECK_TRUE(msg != NULL, "Msg type %u unknown\n", context->type);
  if (msg == NULL) {
    return RFC5444_OKAY;
  }

  CHECK_TRUE(context->msg_flags == msg->flags,
      "Msg %u flags was %x (should be %x)\n",
      msg->type, context->msg_flags, msg->flags);

  CHECK_TRUE(context->addr_len == msg->addrlen,
      "Msg %u addrlen was %u (should be %u)\n",
      msg->type, context->addr_len, msg->addrlen);

  CHECK_TRUE(context->has_origaddr == msg->has_originator,
      "Msg %u has %s originator (should have %s)\n",
      msg->type, context->has_origaddr ? "" : "no ",
      msg->has_originator ? "one" : "none");
  if (context->has_origaddr && context->addr_len == msg->addrlen) {
    CHECK_TRUE(memcmp(context->orig_addr, msg->originator, msg->addrlen) == 0,
        "Msg %u originator was %s (should be %s)\n",
        msg->type, tostring(buf1, context->orig_addr, msg->addrlen),
        tostring(buf2, msg->originator, msg->addrlen));
  }

  CHECK_TRUE(context->has_hopcount == msg->has_hopcount,
      "Msg %u has %s hopcount (should have %s)\n",
      msg->type, context->has_hopcount ? "" : "no ",
      msg->has_hopcount ? "one" : "none");
  if (context->has_hopcount) {
    CHECK_TRUE(context->hopcount == msg->hopcount,
        "Msg %u hopcount was %u (should be %u)\n",
        msg->type, context->hopcount, msg->hopcount);
  }

  CHECK_TRUE(context->has_hoplimit == msg->has_hoplimit,
      "Msg %u has %s hoplimit (should have %s)\n",
      msg->type, context->has_hoplimit ? "" : "no ",
      msg->has_hoplimit ? "one" : "none");
  if (context->has_hoplimit) {
    CHECK_TRUE(context->hoplimit == msg->hoplimit,
        "Msg %u hoplimit was %u (should be %u)\n",
        msg->type, context->hoplimit, msg->hoplimit);
  }

  CHECK_TRUE(context->has_seqno == msg->has_seqno,
      "Msg %u has %s seqno (should have %s)\n",
      msg->type, context->has_seqno ? "" : "no ",
      msg->has_seqno ? "one" : "none");
  if (context->has_seqno) {
    CHECK_TRUE(context->seqno == msg->seqno,
        "Msg %u seqno was %u (should be %u)\n",
        msg->type, context->seqno, msg->seqno);
  }

  msg->okay = true;
  _current_msg = msg;

  return RFC5444_OKAY;
}


static enum rfc5444_result
_msg_end_callback(struct rfc5444_reader_tlvblock_context *context __attribute__((unused)),
    bool dropped __attribute__((unused))) {
  _current_msg = NULL;
  return RFC5444_OKAY;
}

static enum rfc5444_result
_msg_tlv_callback(struct rfc5444_reader_tlvblock_entry *entry,
    struct rfc5444_reader_tlvblock_context *context __attribute__((unused))) {
  struct test_tlv *tlv;
  size_t i;

  for (i=0; i<_current_msg->tlv_count; i++) {
    tlv = &_current_msg->tlvs[i];

    if (tlv->type == entry->type && tlv->exttype == entry->type_ext) {
      if (tlv->length == entry->length
        && memcmp(tlv->value, entry->single_value, tlv->length) == 0) {
        tlv->okay = true;
      }
      else {
        CHECK_TRUE(false, "Pkt TLV %u (ext %u) has wrong value\n",
            entry->type, entry->type_ext);
      }
      return RFC5444_OKAY;
    }
  }

  CHECK_TRUE(false, "Msg %u TLV %u (ext %u) unknown\n",
      _current_msg->type, entry->type, entry->type_ext);
  return RFC5444_OKAY;
}

static enum rfc5444_result
_addr_start_callback(struct rfc5444_reader_tlvblock_context *context) {
  struct test_address *addr;
  char buf1[80];
  size_t i;

  if (_current_msg == NULL) {
    return RFC5444_OKAY;
  }

  addr = NULL;
  for (i=0; i<_current_msg->address_count; i++) {
    if (memcmp(_current_msg->addrs[i].addr, context->addr, _current_msg->addrlen) == 0) {
      addr = &_current_msg->addrs[i];
      break;
    }
  }

  CHECK_TRUE(addr != NULL, "Msg type %u Addr %s unknown\n", context->type,
      tostring(buf1, context->addr, context->addr_len));

  if (addr == NULL) {
    return RFC5444_OKAY;
  }

  CHECK_TRUE(context->prefixlen == addr->plen,
      "Msg type %u Addr %s has plen %u (should be %u)\n", context->type,
      tostring(buf1, context->addr, context->addr_len),
      context->prefixlen, addr->plen);
  addr->okay = true;
  _current_addr = addr;

  return RFC5444_OKAY;
}

static enum rfc5444_result
_addr_end_callback(struct rfc5444_reader_tlvblock_context *context __attribute__((unused)),
    bool dropped __attribute__((unused))) {
  _current_addr = NULL;
  return RFC5444_OKAY;
}

static enum rfc5444_result
_addr_tlv_callback(struct rfc5444_reader_tlvblock_entry *entry,
    struct rfc5444_reader_tlvblock_context *context __attribute__((unused))) {
  struct test_tlv *tlv;
  char buf[80];
  size_t i;

  if (_current_addr == NULL) {
    return RFC5444_OKAY;
  }

  for (i=0; i<_current_addr->tlv_count; i++) {
    tlv = &_current_addr->tlvs[i];

    if (tlv->type == entry->type && tlv->exttype == entry->type_ext) {
      if (tlv->length == entry->length
        && memcmp(tlv->value, entry->single_value, tlv->length) == 0) {
        tlv->okay = true;
      }
      else {
        CHECK_TRUE(false, "Pkt TLV %u (ext %u) has wrong value\n",
            entry->type, entry->type_ext);
      }
      return RFC5444_OKAY;
    }
  }

  CHECK_TRUE(false, "Msg %u Addr %s TLV %u (ext %u) unknown\n",
      _current_msg->type, tostring(buf, context->addr, context->addr_len),
      entry->type, entry->type_ext);
  return RFC5444_OKAY;
}

static void
test_interop2010(struct test_packet *p) {
  enum rfc5444_result result;

  cunit_start_test(p->test);
  _packet = p;

  result = rfc5444_reader_handle_packet(&reader, _packet->binary, _packet->binlen);
  CHECK_TRUE(result == RFC5444_OKAY, "Reader error: %s (%d)",
      rfc5444_strerror(result), result);

  cunit_end_test(p->test);
}

void
add_test(struct test_packet *p) {
  if (_test_tree.comp == NULL) {
    avl_init(&_test_tree, avl_comp_strcasecmp, false);
  }

  p->_node.key = p->test;
  avl_insert(&_test_tree, &p->_node);
}

int
main(int argc __attribute__((unused)), char **argv __attribute__((unused))) {
  struct test_packet *packet;
  BEGIN_TESTING(NULL);

  rfc5444_reader_init(&reader);
  rfc5444_reader_add_packet_consumer(&reader, &_packet_consumer, NULL, 0);
  rfc5444_reader_add_message_consumer(&reader, &_msg_consumer, NULL, 0);
  rfc5444_reader_add_message_consumer(&reader, &_addr_consumer, NULL, 0);

  avl_for_each_element(&_test_tree, packet, _node) {
    test_interop2010(packet);
  }

  rfc5444_reader_cleanup(&reader);

  return FINISH_TESTING();
}


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

#include <string.h>
#include <stdio.h>

#include "common/common_types.h"
#include "common/netaddr.h"
#include "rfc5444/rfc5444_writer.h"

#include "rfc5444_reader_writer/writer.h"

static void _cb_addMessageTLVs(struct rfc5444_writer *wr);
static void _cb_addAddresses(struct rfc5444_writer *wr);

static uint8_t _msg_buffer[128];
static uint8_t _msg_addrtlvs[1000];
static uint8_t _packet_buffer[128];

static struct rfc5444_writer_message *_msg;

/* define dummy interface for generating rfc5444 packets */
struct rfc5444_writer_target interface_1 = {
	.packet_buffer = _packet_buffer,
	.packet_size = sizeof(_packet_buffer),
};

/* define a rfc5444 writer */
struct rfc5444_writer writer = {
	.msg_buffer = _msg_buffer,
	.msg_size = sizeof(_msg_buffer),
	.addrtlv_buffer = _msg_addrtlvs,
	.addrtlv_size = sizeof(_msg_addrtlvs),
};

/*
 * message content provider that will add message tlvs,
 * addresses and address TLVs to all messages of type 1.
 */
static struct rfc5444_writer_content_provider _message_content_provider = {
	.msg_type = 1,
	.addMessageTLVs = _cb_addMessageTLVs,
	.addAddresses = _cb_addAddresses,
};

/* declaration of all address TLVs we will add to the message */
static struct rfc5444_writer_tlvtype addrtlvs[] = {
	{ .type = 0 },
};

/**
 * Callback to add message TLVs to a RFC5444 message
 * @param wr
 */
static void
_cb_addMessageTLVs(struct rfc5444_writer *wr) {
	int foo;
  printf("%s()\n", __func__);

	/* add message tlv type 0 (ext 0) with 4-byte value 23 */
	foo = htonl(23);
	rfc5444_writer_add_messagetlv(wr, 0, 0, &foo, sizeof (foo));

  /* add message tlv type 1 (ext 0) with 4-byte value 42 */
	foo = htonl(42);
	rfc5444_writer_add_messagetlv(wr, 1, 0, &foo, sizeof (foo));

	/* add message tlv type 1 (ext 0) with 4-byte value 5 */
	foo = htonl(5);
	rfc5444_writer_add_messagetlv(wr, 1, 0, &foo, sizeof (foo));
}

/**
 * Callback to add addresses and address TLVs to a RFC5444 message
 * @param wr
 */
static void
_cb_addAddresses(struct rfc5444_writer *wr) {
  struct rfc5444_writer_address *addr;
  struct netaddr ip0, ip1;
	int value;;

  if (netaddr_from_string(&ip0, "127.0.0.1")) {
    return;
  }
  if (netaddr_from_string(&ip1, "127.0.0.42")) {
    return;
  }

  value = htonl(2001);

	/* add an address with a tlv attached */
	addr = rfc5444_writer_add_address(wr, _message_content_provider.creator, &ip0, false);

	/* add an address TLV to the new address */
	rfc5444_writer_add_addrtlv(wr, addr, &addrtlvs[0], &value, sizeof (value), false);

	/* add an address without an tvl */
	rfc5444_writer_add_address(wr, _message_content_provider.creator, &ip1, false);
}

/**
 * Callback to define the message header for a RFC5444 message
 * @param wr
 * @param message
 */
static void
_cb_addMessageHeader(struct rfc5444_writer *wr, struct rfc5444_writer_message *message) {
  printf("%s()\n", __func__);

	/* no originator, no sequence number, not hopcount, no hoplimit */
	rfc5444_writer_set_msg_header(wr, message, false, false, false, false);
}

/**
 * Initialize RFC5444 writer
 * @param ptr pointer to "send_packet" function
 */
void
writer_init(write_packet_func_ptr ptr) {
  printf("%s()\n", __func__);

  /* initialize writer */
  rfc5444_writer_init(&writer);

  /* register a target (for sending messages to) in writer */
  rfc5444_writer_register_target(&writer, &interface_1);

  /* register a message content provider */
  rfc5444_writer_register_msgcontentprovider(&writer, &_message_content_provider, addrtlvs, ARRAYSIZE(addrtlvs));

  /* register message type 1 with 4 byte addresses */
  _msg = rfc5444_writer_register_message(&writer, 1, false, 4);
  _msg->addMessageHeader = _cb_addMessageHeader;

  /* set function to send binary packet content */
  interface_1.sendPacket = ptr;
}

/**
 * Cleanup RFC5444 writer
 */
void
writer_cleanup(void) {
  printf("%s()\n", __func__);

  rfc5444_writer_cleanup(&writer);
}

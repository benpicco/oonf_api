#include <string.h>
#include <stdio.h>

#include "common/common_types.h"
#include "common/netaddr.h"
#include "rfc5444/rfc5444_writer.h"

#include "rfc5444_reader_writer/writer.h"

static void _cb_addMessageTLVs(struct rfc5444_writer *wr);

static uint8_t _msg_buffer[128];
static uint8_t _msg_addrtlvs[1000];
static uint8_t _packet_buffer[128];

static struct rfc5444_writer_message *_msg;

struct rfc5444_writer_target interface_1 = {
	.packet_buffer = _packet_buffer,
	.packet_size = sizeof(_packet_buffer),
};

struct rfc5444_writer writer = {
	.msg_buffer = _msg_buffer,
	.msg_size = sizeof(_msg_buffer),
	.addrtlv_buffer = _msg_addrtlvs,
	.addrtlv_size = sizeof(_msg_addrtlvs),
};

static struct rfc5444_writer_content_provider _message_content_provider = {
	.msg_type = 1,
	.addMessageTLVs = _cb_addMessageTLVs,
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
  rfc5444_writer_register_msgcontentprovider(&writer, &_message_content_provider, 0, 0);

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

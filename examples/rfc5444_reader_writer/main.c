#include <string.h>
#include <stdio.h>

#include "common/common_types.h"
#include "common/netaddr.h"

#include "rfc5444/rfc5444_reader.h"
#include "rfc5444/rfc5444_writer.h"
#include "rfc5444/rfc5444_print.h"

#include "rfc5444_reader_writer/reader.h"
#include "rfc5444_reader_writer/writer.h"

static struct autobuf _hexbuf;

/**
 * Handle the output of the RFC5444 packet creation process
 * @param wr
 * @param iface
 * @param buffer
 * @param length
 */
static void
write_packet(struct rfc5444_writer *wr __attribute__ ((unused)),
    struct rfc5444_writer_target *iface __attribute__((unused)),
    void *buffer, size_t length) {
  printf("%s()\n", __func__);

  /* generate hexdump of packet */
  rfc5444_print_hexdump(&_hexbuf, "\t", buffer, length);
  rfc5444_print_direct(&_hexbuf, buffer, length);

  /* print hexdump to console */
  printf("%s", abuf_getptr(&_hexbuf));

  /* parse packet */
  rfc5444_reader_handle_packet(&reader, buffer, length);
}

int main(int argc __attribute__ ((unused)), char **argv __attribute__ ((unused))) {
  /* initialize buffer for hexdump */
  abuf_init(&_hexbuf);

  /* init reader and writer */
  reader_init();
  writer_init(write_packet);
  
  /* send message */
  rfc5444_writer_create_message_alltarget(&writer, 1);
  rfc5444_writer_flush(&writer, &interface_1, false);

  /* cleanup */
  reader_cleanup();
  writer_cleanup();
  abuf_free(&_hexbuf);

  return 0;
}

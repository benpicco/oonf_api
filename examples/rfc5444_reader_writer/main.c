
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

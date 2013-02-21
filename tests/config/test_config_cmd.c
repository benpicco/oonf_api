
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2021, the olsr.org team - see HISTORY file
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

#include "common/autobuf.h"
#include "config/cfg_db.h"
#include "config/cfg_cmd.h"

#include "cunit/cunit.h"

struct cfg_db *db = NULL;
struct cfg_instance instance;
struct autobuf log_buf;

static void
clear_elements(void) {
  if (db) {
    cfg_db_remove(db);
  }
  db = cfg_db_add();
  cfg_cmd_clear_state(&instance);
  abuf_clear(&log_buf);
}

static void
test_cmd_sections(void) {
  struct cfg_section_type *stype1, *stype2;
  struct cfg_named_section *named;

  START_TEST();

  CHECK_TRUE(0 == cfg_cmd_handle_set(&instance, db, "stype1.", &log_buf),
      "Error while adding sectiontype 'stype1': %s", abuf_getptr(&log_buf));

  stype1 = cfg_db_find_sectiontype(db, "stype1");
  CHECK_TRUE(stype1 != NULL, "'stype1' not found");
  if (stype1) {
    CHECK_TRUE(stype1->names.count == 1, "Number of named sections is not 1: %u",
        stype1->names.count);

    named = cfg_db_get_unnamed_section(stype1);
    CHECK_TRUE(named != NULL, "Named section does not have the default name.");
  }

  CHECK_TRUE(0 == cfg_cmd_handle_set(&instance, db, "stype2.", &log_buf),
      "Error while adding sectiontype 'stype2': %s", abuf_getptr(&log_buf));

  stype2 = cfg_db_find_sectiontype(db, "stype2");
  CHECK_TRUE(stype2 != NULL, "'stype2' not found");
  if (stype2) {
    CHECK_TRUE(stype2->names.count == 1, "Number of named sections is not 1: %u",
        stype2->names.count);

    named = cfg_db_get_unnamed_section(stype2);
    CHECK_TRUE(named != NULL, "Named section does not have the default name.");
  }

  CHECK_TRUE(0 == cfg_cmd_handle_remove(&instance, db, "stype2.", &log_buf),
      "Error while removing sectiontype 'stype2': %s", abuf_getptr(&log_buf));

  stype2 = cfg_db_find_sectiontype(db, "stype2");
  CHECK_TRUE(stype2 == NULL, "'stype2' is still in the database");

  END_TEST();
}


static void
test_cmd_namedsections(void) {
  struct cfg_named_section *stype1, *stype2;

  START_TEST();

  CHECK_TRUE(0 == cfg_cmd_handle_set(&instance, db, "stype1[name].", &log_buf),
      "Error while adding sectiontype 'stype1[name]'");

  stype1 = cfg_db_find_namedsection(db, "stype1", "name");
  CHECK_TRUE(stype1 != NULL, "'stype1[name]' not found");
  if (stype1) {
    CHECK_TRUE(strcmp(stype1->name, "name") == 0, "name of section 1 wrong: %s", stype1->name);
  }

  CHECK_TRUE(0 == cfg_cmd_handle_set(&instance, db, "stype2[name2].", &log_buf),
      "Error while adding sectiontype 'stype2[name2]'");

  stype2 = cfg_db_find_namedsection(db, "stype2", "name2");
  CHECK_TRUE(stype2 != NULL, "'stype2[name]' not found");
  if (stype2) {
    CHECK_TRUE(strcmp(stype2->name, "name2") == 0, "name of section 2 wrong: %s", stype2->name);
  }

  CHECK_TRUE(0 == cfg_cmd_handle_remove(&instance, db, "stype2[name2].", &log_buf),
      "Error while removing sectiontype 'stype2[name2]'");

  stype2 = cfg_db_find_namedsection(db, "stype2", "name2");
  CHECK_TRUE(stype2 == NULL, "'stype2' is still in the database");

  END_TEST();
}

static void
test_cmd_entries(void) {
  struct cfg_entry *stype1, *stype2;

  START_TEST();

  CHECK_TRUE(0 == cfg_cmd_handle_set(&instance, db, "stype1[name1].key1=v1", &log_buf),
      "Error while adding entry 'stype1[name1].key1'");

  stype1 = cfg_db_find_entry(db, "stype1", "name1", "key1");
  CHECK_TRUE(stype1 != NULL, "'stype1[name1].key1' not found");
  if (stype1) {
    CHECK_TRUE(strcmp(stype1->val.value, "v1") == 0, "Value does not match 'v1': %s", stype1->val.value);
  }

  CHECK_TRUE(0 == cfg_cmd_handle_set(&instance, db, "stype2[name2].key2=v2", &log_buf),
      "Error while adding entry 'stype2[name2].key2'");

  stype2 = cfg_db_find_entry(db, "stype2", "name2", "key2");
  CHECK_TRUE(stype2 != NULL, "'stype2[name2].key2' not found");
  if (stype2) {
    CHECK_TRUE(strcmp(stype2->val.value, "v2") == 0, "Value does not match 'v2': %s", stype2->val.value);
  }


  CHECK_TRUE(0 == cfg_cmd_handle_remove(&instance, db, "stype2[name2].key2", &log_buf),
      "Error while removing entry 'stype2[name2].key2'");

  stype2 = cfg_db_find_entry(db, "stype2", "name2", "key2");
  CHECK_TRUE(stype2 == NULL, "'stype2[name2].key2' is still in the database");

  END_TEST();
}

int
main(int argc __attribute__ ((unused)), char **argv __attribute__ ((unused))) {
  abuf_init(&log_buf);

  cfg_add(&instance);

  BEGIN_TESTING(clear_elements);

  test_cmd_sections();
  test_cmd_namedsections();
  test_cmd_entries();

  abuf_free(&log_buf);
  cfg_db_remove(db);
  cfg_remove(&instance);

  return FINISH_TESTING();
}



/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
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

#include <stdio.h>

#include "config/cfg_schema.h"
#include "config/cfg_db.h"

#include "cunit/cunit.h"

static struct cfg_schema schema;

static struct cfg_schema_entry entries[] = {
    CFG_VALIDATE_PRINTABLE("key1", "default", "helptext list", .list = true),
};

static struct cfg_schema_section section =
{
  .type = "section1", .mode = CFG_SSMODE_NAMED,
  .entries = entries,
  .entry_count = ARRAYSIZE(entries),
};


static void
test_list_1(void) {
  struct cfg_db *db;
  struct cfg_entry *entry1, *entry2, *entry3;
  char *ptr;
  int cnt;
  START_TEST();

  db = cfg_db_add();

  /* set initial entry */
  entry1 = cfg_db_set_entry(db, "section1", "testname", "key1", "test 1", false);

  /* add a second entry */
  entry2 = cfg_db_set_entry(db, "section1", "testname", "key1", "test 2", true);

  /* add a third entry */
  entry3 = cfg_db_set_entry(db, "section1", "testname", "key1", "test 3", true);

  CHECK_TRUE(entry1 == entry2 && entry1 == entry3, "append did create more than one value");
  CHECK_TRUE(cfg_db_is_multipart_entry(entry1), "Error, append did not create a multipart value");

  cnt = 0;
  FOR_ALL_STRINGS(&entry1->val, ptr) {
    cnt++;
    CHECK_TRUE(cnt < 4, "append did create more than three entries: %d", cnt);

    if (cnt == 3) {
      CHECK_TRUE(strcmp(ptr, "test 1") == 0, "part %d was not 'test 1' but '%s'", cnt, ptr);
    }
    if (cnt == 2) {
      CHECK_TRUE(strcmp(ptr, "test 2") == 0, "part %d was not 'test 2' but '%s'", cnt, ptr);
    }
    if (cnt == 1) {
      CHECK_TRUE(strcmp(ptr, "test 3") == 0, "part %d was not 'test 3' but '%s'", cnt, ptr);
    }
  }

  CHECK_TRUE(cnt == 3, "append did not create three entries: %d", cnt);
  CHECK_TRUE(cfg_db_entry_get_listsize(entry1) == 3,
      "append did not create three entries: %"PRINTF_SIZE_T_SPECIFIER" (get)", cfg_db_entry_get_listsize(entry1));

  CHECK_TRUE(cfg_db_remove_element(db, "section1", "testname", "key1", "test 1") == 0,
      "could not remove first entry");

  cnt = 0;
  FOR_ALL_STRINGS(&entry1->val, ptr) {
    cnt++;
    CHECK_TRUE(cnt < 3, "append+remove did create more than two entries: %d", cnt);

    if (cnt == 2) {
      CHECK_TRUE(strcmp(ptr, "test 2") == 0, "part %d was not 'test 2' but '%s'", cnt, ptr);
    }
    if (cnt == 1) {
      CHECK_TRUE(strcmp(ptr, "test 3") == 0, "part %d was not 'test 3' but '%s'", cnt, ptr);
    }
  }

  CHECK_TRUE(cnt == 2, "append+remove did not create two entries: %d", cnt);
  CHECK_TRUE(cfg_db_entry_get_listsize(entry1) == 2,
      "append did not create two entries: %"PRINTF_SIZE_T_SPECIFIER" (get)", cfg_db_entry_get_listsize(entry1));

  cfg_db_remove(db);
  END_TEST();
}

static void
test_list_2(void) {
  struct cfg_db *db;
  struct cfg_entry *entry1, *entry2, *entry3;
  char *ptr;
  int cnt;
  START_TEST();

  db = cfg_db_add();

  /* set initial entry */
  entry1 = cfg_db_set_entry(db, "section1", "testname", "key1", "test 1", false);

  /* add a second entry */
  entry2 = cfg_db_set_entry(db, "section1", "testname", "key1", "test 2", true);

  /* add a third entry */
  entry3 = cfg_db_set_entry(db, "section1", "testname", "key1", "test 3", true);

  CHECK_TRUE(entry1 == entry2 && entry1 == entry3, "append did create more than one value");
  CHECK_TRUE(cfg_db_is_multipart_entry(entry1), "Error, append did not create a multipart value");

  cnt = 0;
  FOR_ALL_STRINGS(&entry1->val, ptr) {
    cnt++;
    CHECK_TRUE(cnt < 4, "append did create more than three entries: %d", cnt);

    if (cnt == 3) {
      CHECK_TRUE(strcmp(ptr, "test 1") == 0, "part %d was not 'test 1' but '%s'", cnt, ptr);
    }
    if (cnt == 2) {
      CHECK_TRUE(strcmp(ptr, "test 2") == 0, "part %d was not 'test 2' but '%s'", cnt, ptr);
    }
    if (cnt == 1) {
      CHECK_TRUE(strcmp(ptr, "test 3") == 0, "part %d was not 'test 3' but '%s'", cnt, ptr);
    }
  }

  CHECK_TRUE(cnt == 3, "append did not create three entries: %d", cnt);
  CHECK_TRUE(cfg_db_entry_get_listsize(entry1) == 3,
      "append did not create three entries: %"PRINTF_SIZE_T_SPECIFIER" (get)", cfg_db_entry_get_listsize(entry1));

  CHECK_TRUE(cfg_db_remove_element(db, "section1", "testname", "key1", "test 2") == 0,
      "could not remove first entry");

  cnt = 0;
  FOR_ALL_STRINGS(&entry1->val, ptr) {
    cnt++;
    CHECK_TRUE(cnt < 3, "append+remove did create more than two entries: %d", cnt);

    if (cnt == 2) {
      CHECK_TRUE(strcmp(ptr, "test 1") == 0, "part %d was not 'test 1' but '%s'", cnt, ptr);
    }
    if (cnt == 1) {
      CHECK_TRUE(strcmp(ptr, "test 3") == 0, "part %d was not 'test 3' but '%s'", cnt, ptr);
    }
  }

  CHECK_TRUE(cnt == 2, "append+remove did not create two entries: %d", cnt);
  CHECK_TRUE(cfg_db_entry_get_listsize(entry1) == 2,
      "append did not create two entries: %"PRINTF_SIZE_T_SPECIFIER" (get)", cfg_db_entry_get_listsize(entry1));

  cfg_db_remove(db);
  END_TEST();
}

static void
test_list_3(void) {
  struct cfg_db *db;
  struct cfg_entry *entry1, *entry2, *entry3;
  char *ptr;
  int cnt;
  START_TEST();

  db = cfg_db_add();

  /* set initial entry */
  entry1 = cfg_db_set_entry(db, "section1", "testname", "key1", "test 1", false);

  /* add a second entry */
  entry2 = cfg_db_set_entry(db, "section1", "testname", "key1", "test 2", true);

  /* add a third entry */
  entry3 = cfg_db_set_entry(db, "section1", "testname", "key1", "test 3", true);

  CHECK_TRUE(entry1 == entry2 && entry1 == entry3, "append did create more than one value");
  CHECK_TRUE(cfg_db_is_multipart_entry(entry1), "Error, append did not create a multipart value");

  cnt = 0;
  FOR_ALL_STRINGS(&entry1->val, ptr) {
    cnt++;
    CHECK_TRUE(cnt < 4, "append did create more than three entries: %d", cnt);

    if (cnt == 3) {
      CHECK_TRUE(strcmp(ptr, "test 1") == 0, "part %d was not 'test 1' but '%s'", cnt, ptr);
    }
    if (cnt == 2) {
      CHECK_TRUE(strcmp(ptr, "test 2") == 0, "part %d was not 'test 2' but '%s'", cnt, ptr);
    }
    if (cnt == 1) {
      CHECK_TRUE(strcmp(ptr, "test 3") == 0, "part %d was not 'test 3' but '%s'", cnt, ptr);
    }
  }

  CHECK_TRUE(cnt == 3, "append did not create three entries: %d", cnt);
  CHECK_TRUE(cfg_db_entry_get_listsize(entry1) == 3,
      "append did not create three entries: %"PRINTF_SIZE_T_SPECIFIER" (get)", cfg_db_entry_get_listsize(entry1));

  CHECK_TRUE(cfg_db_remove_element(db, "section1", "testname", "key1", "test 3") == 0,
      "could not remove first entry");

  cnt = 0;
  FOR_ALL_STRINGS(&entry1->val, ptr) {
    cnt++;
    CHECK_TRUE(cnt < 3, "append+remove did create more than two entries: %d", cnt);

    if (cnt == 2) {
      CHECK_TRUE(strcmp(ptr, "test 1") == 0, "part %d was not 'test 1' but '%s'", cnt, ptr);
    }
    if (cnt == 1) {
      CHECK_TRUE(strcmp(ptr, "test 2") == 0, "part %d was not 'test 2' but '%s'", cnt, ptr);
    }
  }

  CHECK_TRUE(cnt == 2, "append+remove did not create two entries: %d", cnt);
  CHECK_TRUE(cfg_db_entry_get_listsize(entry1) == 2,
      "append did not create two entries: %"PRINTF_SIZE_T_SPECIFIER" (get)", cfg_db_entry_get_listsize(entry1));

  cfg_db_remove(db);
  END_TEST();
}

int
main(int argc __attribute__ ((unused)), char **argv __attribute__ ((unused))) {
  cfg_schema_add(&schema);
  cfg_schema_add_section(&schema, &section);

  BEGIN_TESTING(NULL);

  test_list_1();
  test_list_2();
  test_list_3();

  cfg_schema_remove_section(&schema, &section);

  return FINISH_TESTING();
}

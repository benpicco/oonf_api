
/*
 * The olsr.org Optimized Link-State Routing daemon(olsrd)
 * Copyright (c) 2004-2012, the olsr.org team - see HISTORY file
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

#include "config/cfg_db.h"
#include "config/cfg_schema.h"

#include "common/autobuf.h"
#include "common/netaddr.h"
#include "common/string.h"
#include "config/cfg_db.h"
#include "config/cfg_schema.h"

#include "cunit/cunit.h"

#define CFG_SECTION "sec_type"
#define CFG_SECTION_NAME "sec_name"
#define CFG_ENTRY_DEF "entry"
#define CFG_ENTRY_NODEF "nodefentry"

#define CFG_SCHEMA_DEFAULT "schema_default"
#define CFG_UNNAMED_VALUE  "unnamed_value"
#define CFG_NAMED_VALUE    "named_value"

static struct cfg_db *db = NULL;
static struct autobuf out;

static struct cfg_schema schema;

static struct cfg_schema_section section = {
  .type = CFG_SECTION, .mode = CFG_SSMODE_NAMED
};

static struct cfg_schema_entry entries[] = {
    CFG_VALIDATE_STRING(CFG_ENTRY_DEF, CFG_SCHEMA_DEFAULT, "help string"),
    CFG_VALIDATE_STRING(CFG_ENTRY_NODEF, NULL, "help string"),
};

static void
clear_elements(void) {
  if (db) {
    cfg_db_remove(db);
  }

  db = cfg_db_add();
  cfg_db_link_schema(db, &schema);

  abuf_clear(&out);
}

static void
test_default_named_section_set(void) {
  const struct const_strarray *value;
  START_TEST();

  cfg_db_overwrite_entry(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_DEF, CFG_NAMED_VALUE);
  cfg_db_overwrite_entry(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_NODEF, CFG_NAMED_VALUE);

  value = cfg_db_get_entry_value(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_DEF);
  CHECK_TRUE(value != NULL, "No value found for named section entry with default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_NAMED_VALUE) == 0,
          "Did not got the named_section value with default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_NODEF);
  CHECK_TRUE(value != NULL, "No value found for named section entry without default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_NAMED_VALUE) == 0,
          "Did not got the named_section value without default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, NULL, CFG_ENTRY_DEF);
  CHECK_TRUE(value != NULL, "No value found for unnamed section entry with default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_SCHEMA_DEFAULT) == 0,
          "Did not got the named_section value with default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, NULL, CFG_ENTRY_NODEF);
  CHECK_TRUE(value == NULL, "value found for unnamed section entry without default: %s",
      value == NULL ? "" : value->value);
  END_TEST();
}

static void
test_default_unnamed_named_section_set(void) {
  const struct const_strarray *value;
  START_TEST();

  cfg_db_overwrite_entry(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_DEF, CFG_NAMED_VALUE);
  cfg_db_overwrite_entry(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_NODEF, CFG_NAMED_VALUE);

  cfg_db_overwrite_entry(db, CFG_SECTION, NULL, CFG_ENTRY_DEF, CFG_UNNAMED_VALUE);
  cfg_db_overwrite_entry(db, CFG_SECTION, NULL, CFG_ENTRY_NODEF, CFG_UNNAMED_VALUE);

  value = cfg_db_get_entry_value(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_DEF);
  CHECK_TRUE(value != NULL, "No value found for named section entry with default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_NAMED_VALUE) == 0,
          "Did not got the named_section value with default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_NODEF);
  CHECK_TRUE(value != NULL, "No value found for named section entry without default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_NAMED_VALUE) == 0,
          "Did not got the named_section value without default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, NULL, CFG_ENTRY_DEF);
  CHECK_TRUE(value != NULL, "No value found for unnamed section entry with default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_UNNAMED_VALUE) == 0,
          "Did not got the unnamed_section value with default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, NULL, CFG_ENTRY_NODEF);
  CHECK_TRUE(value != NULL, "No value found for unnamed section entry without default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_UNNAMED_VALUE) == 0,
          "Did not got the unnamed_section value without default: %s", value->value);
  }

  END_TEST();
}

static void
test_default_nothing_set(void) {
  const struct const_strarray *value;
  START_TEST();

  value = cfg_db_get_entry_value(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_DEF);
  CHECK_TRUE(value != NULL, "No value found for named section entry with default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_SCHEMA_DEFAULT) == 0,
          "Did not got the named_section value with default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_NODEF);
  CHECK_TRUE(value == NULL, "value found for named section entry without default");

  value = cfg_db_get_entry_value(db, CFG_SECTION, NULL, CFG_ENTRY_DEF);
  CHECK_TRUE(value != NULL, "No value found for unnamed section entry with default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_SCHEMA_DEFAULT) == 0,
          "Did not got the named_section value with default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, NULL, CFG_ENTRY_NODEF);
  CHECK_TRUE(value == NULL, "value found for unnamed section entry without default: %s",
      value == NULL ? "" : value->value);
  END_TEST();
}

static void
test_default_unnamed_section_set(void) {
  const struct const_strarray *value;
  START_TEST();

  cfg_db_overwrite_entry(db, CFG_SECTION, NULL, CFG_ENTRY_DEF, CFG_UNNAMED_VALUE);
  cfg_db_overwrite_entry(db, CFG_SECTION, NULL, CFG_ENTRY_NODEF, CFG_UNNAMED_VALUE);

  value = cfg_db_get_entry_value(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_DEF);
  CHECK_TRUE(value != NULL, "No value found for named section entry with default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_UNNAMED_VALUE) == 0,
          "Did not got the named_section value with default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, CFG_SECTION_NAME, CFG_ENTRY_NODEF);
  CHECK_TRUE(value != NULL, "No value found for named section entry without default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_UNNAMED_VALUE) == 0,
          "Did not got the named_section value without default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, NULL, CFG_ENTRY_DEF);
  CHECK_TRUE(value != NULL, "No value found for unnamed section entry with default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_UNNAMED_VALUE) == 0,
          "Did not got the named_section value with default: %s", value->value);
  }

  value = cfg_db_get_entry_value(db, CFG_SECTION, NULL, CFG_ENTRY_NODEF);
  CHECK_TRUE(value != NULL, "No value found for unnamed section entry without default");
  if (value) {
      CHECK_TRUE(value->value != NULL && strcmp(value->value, CFG_UNNAMED_VALUE) == 0,
          "Did not got the unnamed_section value without default: %s", value->value);
  }
  END_TEST();
}

int
main(int argc __attribute__ ((unused)), char **argv __attribute__ ((unused))) {
  cfg_schema_add(&schema);
  cfg_schema_add_section(&schema, &section, entries, ARRAYSIZE(entries));

  abuf_init(&out);
  BEGIN_TESTING(clear_elements);

  test_default_named_section_set();
  test_default_unnamed_named_section_set();
  test_default_unnamed_section_set();
  test_default_nothing_set();

  abuf_free(&out);
  if (db) {
    cfg_db_remove(db);
  }

  return FINISH_TESTING();
}

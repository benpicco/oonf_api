
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

#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common/netaddr.h"

#include "common/avl.h"
#include "common/avl_comp.h"
#include "common/string.h"

#include "config/cfg.h"
#include "config/cfg_db.h"
#include "config/cfg_schema.h"

static bool _validate_cfg_entry(
    struct cfg_db *db, struct cfg_section_type *section,
    struct cfg_named_section *named, struct cfg_entry *entry,
    const char *section_name, bool cleanup, struct autobuf *out);
static bool _check_missing_entries(struct cfg_schema_section *schema_section,
    struct cfg_db *db, struct cfg_named_section *named,
    const char *section_name, struct autobuf *out);
static void _handle_named_section_change(struct cfg_schema_section *s_section,
    struct cfg_db *pre_change, struct cfg_db *post_change,
    const char *name, bool startup);
static int _handle_db_changes(struct cfg_db *pre_change,
    struct cfg_db *post_change, bool startup);
static int _get_known_prefix(struct netaddr *dst, const char *name);

const char *CFGLIST_BOOL_TRUE[] = { "true", "1", "on", "yes" };
const char *CFGLIST_BOOL[] = { "true", "1", "on", "yes", "false", "0", "off", "no" };
const char *CFG_SCHEMA_SECTIONMODE[CFG_SSMODE_MAX] = {
    "unnamed",
    "unnamed, optional",
    "named",
    "named, mandatory"
};

/* List of predefined address prefixes */
const struct {
  const char *name;
  const struct netaddr *prefix;
} _known_prefixes[] = {
  { NETADDR_STR_ANY4, &NETADDR_IPV4_ANY },
  { NETADDR_STR_ANY6, &NETADDR_IPV6_ANY },
  { NETADDR_STR_LINKLOCAL4, &NETADDR_IPV4_LINKLOCAL },
  { NETADDR_STR_LINKLOCAL6, &NETADDR_IPV6_LINKLOCAL },
  { NETADDR_STR_ULA, &NETADDR_IPV6_ULA },
};

/**
 * Initialize a schema
 * @param schema pointer to uninitialized schema
 */
void
cfg_schema_add(struct cfg_schema *schema) {
  avl_init(&schema->sections, cfg_avlcmp_keys, true);
  avl_init(&schema->entries, cfg_avlcmp_schemaentries, true);
  avl_init(&schema->handlers, avl_comp_uint32, true);
}

/**
 * Add a section to a schema
 * @param schema pointer to configuration schema
 * @param section pointer to section
 * @param entries pointer to array of schema entries
 * @param entry_count number of schema entries in array
 */
void
cfg_schema_add_section(struct cfg_schema *schema,
    struct cfg_schema_section *section,
    struct cfg_schema_entry *entries, size_t entry_count) {
  struct cfg_schema_entry *entry, *entry_it;
  size_t i;

  section->_section_node.key = section->type;
  avl_insert(&schema->sections, &section->_section_node);

  if (section->cb_delta_handler) {
    section->_delta_node.key = &section->delta_priority;
    avl_insert(&schema->handlers, &section->_delta_node);
  }

  section->_entries = entries;
  section->_entry_count = entry_count;

  for (i=0; i<entry_count; i++) {
    entries[i]._parent = section;
    entries[i].key.type = section->type;
    entries[i]._node.key = &entries[i].key;

    /* make sure all defaults are the same */
    avl_for_each_elements_with_key(&schema->entries, entry_it, _node, entry,
        &entries[i].key) {
      if (entries[i].def.value == NULL) {
        /* if we have no default, copy the one from the first existing entry */
        memcpy(&entries[i].def, &entry->def, sizeof(entry->def));
        break;
      }
      else {
        /* if we have one, overwrite all existing entries */
        memcpy(&entry->def, &entries[i].def, sizeof(entry->def));

        // TODO: maybe output some logging that we overwrite the default?
      }
    }
    avl_insert(&schema->entries, &entries[i]._node);
  }
}

/**
 * Removes a section from a schema
 * @param schema pointer to configuration schema
 * @param section pointer to section
 */
void
cfg_schema_remove_section(struct cfg_schema *schema, struct cfg_schema_section *section) {
  size_t i;

  if (section->_section_node.key) {
    avl_remove(&schema->sections, &section->_section_node);
    section->_section_node.key = NULL;

    for (i=0; i<section->_entry_count; i++) {
      avl_remove(&schema->entries, &section->_entries[i]._node);
      section->_entries[i]._node.key = NULL;
    }
  }
  if (section->_delta_node.key) {
    avl_remove(&schema->handlers, &section->_delta_node);
    section->_delta_node.key = NULL;
  }
}

/**
 * Validates a database with a schema
 * @param db pointer to configuration database
 * @param cleanup if true, bad values will be removed from the database
 * @param ignore_unknown_sections true if the validation should skip sections
 *   in the database that have no schema.
 * @param out autobuffer for validation output
 * @return 0 if validation found no problems, -1 otherwise
 */
int
cfg_schema_validate(struct cfg_db *db,
    bool cleanup, bool ignore_unknown_sections,
    struct autobuf *out) {
  char section_name[256];
  struct cfg_section_type *section, *section_it;
  struct cfg_named_section *named, *named_it;
  struct cfg_entry *entry, *entry_it;

  struct cfg_schema_section *schema_section;
  struct cfg_schema_section *schema_section_first, *schema_section_last;

  bool error = false;
  bool warning = false;
  bool hasName = false;

  if (db->schema == NULL) {
    return -1;
  }

  CFG_FOR_ALL_SECTION_TYPES(db, section, section_it) {
    /* check for missing schema sections */
    schema_section_first = avl_find_element(&db->schema->sections, section->type,
        schema_section_first, _section_node);

    if (schema_section_first == NULL) {
      if (ignore_unknown_sections) {
        continue;
      }

      cfg_append_printable_line(out,
          "Cannot find schema for section type '%s'", section->type);

      if (cleanup) {
        cfg_db_remove_sectiontype(db, section->type);
      }

      error |= true;
      continue;
    }

    schema_section_last = avl_find_le_element(&db->schema->sections, section->type,
        schema_section_last, _section_node);

    /* iterate over all schema for a certain section type */
    avl_for_element_range(schema_section_first, schema_section_last, schema_section, _section_node) {
      /* check data of named sections in db */
      CFG_FOR_ALL_SECTION_NAMES(section, named, named_it) {
        warning = false;
        hasName = cfg_db_is_named_section(named);

        if (!(schema_section->mode == CFG_SSMODE_NAMED
            || schema_section->mode == CFG_SSMODE_NAMED_MANDATORY) && hasName) {
          cfg_append_printable_line(out, "The section type '%s'"
              " has to be used without a name"
              " ('%s' was given as a name)", section->type, named->name);

          warning = true;
        }

        if (hasName && !cfg_is_allowed_key(named->name, true)) {
          cfg_append_printable_line(out, "The section name '%s' for"
              " type '%s' contains illegal characters",
              named->name, section->type);
          warning = true;
        }

        /* test abort condition */
        if (warning && cleanup) {
          /* remove bad named section */
          cfg_db_remove_namedsection(db, section->type, named->name);
        }

        error |= warning;

        if (warning) {
          continue;
        }

        /* initialize section_name field for validate */
        snprintf(section_name, sizeof(section_name), "'%s%s%s'",
            section->type, hasName ? "=" : "", hasName ? named->name : "");

        /* check for bad values */
        CFG_FOR_ALL_ENTRIES(named, entry, entry_it) {
          warning = _validate_cfg_entry(
              db, section, named, entry, section_name,
              cleanup, out);
          error |= warning;
        }

        /* check for missing values */
        warning = _check_missing_entries(schema_section, db, named, section_name, out);
        error |= warning;

        /* check custom section validation if everything was fine */
        if (!error && schema_section->cb_validate != NULL) {
          if (schema_section->cb_validate(section_name, named, out)) {
            error = true;
          }
        }
      }
    }
    if (cleanup && avl_is_empty(&section->names)) {
      /* if section type is empty, remove it too */
      cfg_db_remove_sectiontype(db, section->type);
    }
  }

  /* search for missing mandatory sections */
  avl_for_each_element(&db->schema->sections, schema_section, _section_node) {
    if (schema_section->mode != CFG_SSMODE_NAMED_MANDATORY) {
      continue;
    }

    section = cfg_db_find_sectiontype(db, schema_section->type);
    if (section == NULL || avl_is_empty(&section->names)) {
      warning = true;
    }
    else {
      named = avl_first_element(&section->names, named, node);
      warning = !cfg_db_is_named_section(named) && section->names.count < 2;
    }
    if (warning) {
      cfg_append_printable_line(out, "Missing mandatory section of type '%s'",
          schema_section->type);
    }
    error |= warning;
  }
  return error ? -1 : 0;
}

/**
 * Convert the entries of a db section into binary representation by
 * using the mappings defined in a schema section. The function assumes
 * that the section was already validated.
 * @param target pointer to target binary buffer
 * @param named pointer to named section, might be NULL to refer to
 *   default settings
 * @param entries pointer to array of schema entries
 * @param count number of schema entries
 * @return 0 if conversion was successful, -(1+index) of the
 *   failed conversion array entry if an error happened.
 *   An error might result in a partial initialized target buffer.
 */
int
cfg_schema_tobin(void *target, struct cfg_named_section *named,
    const struct cfg_schema_entry *entries, size_t count) {
  char *ptr;
  size_t i;
  const struct const_strarray *value;

  ptr = (char *)target;

  for (i=0; i<count; i++) {
    if (entries[i].cb_to_binary == NULL) {
      continue;
    }

    /* cleanup pointer */
    if (named) {
      value = cfg_db_get_entry_value(
          named->section_type->db,
          named->section_type->type,
          named->name,
          entries[i].key.entry);
    }
    else {
      value = &entries[i].def;
    }

    if (entries[i].cb_to_binary(&entries[i], value, ptr + entries[i].bin_offset)) {
      /* error in conversion */
      return -1-i;
    }
  }
  return 0;
}

/**
 * Compare two databases with the same schema and call their change listeners
 * @param pre_change database before change
 * @param post_change database after change
 * @return -1 if databases have different schema, 0 otherwise
 */
int
cfg_schema_handle_db_changes(struct cfg_db *pre_change, struct cfg_db *post_change) {
  return _handle_db_changes(pre_change, post_change, false);
}

/**
 * Handle trigger of delta callbacks on program startup. Call every trigger
 * except for CFG_SSMODE_UNNAMED_OPTIONAL_STARTUP_TRIGGER mode.
 * @param post_db pointer to new configuration database
 * @return -1 if an error happened, 0 otherwise
 */
int
cfg_schema_handle_db_startup_changes(struct cfg_db *post_db) {
  struct cfg_db *pre_db;
  int result;

  pre_db = cfg_db_add();
  if (pre_db == NULL) {
    return -1;
  }
  cfg_db_link_schema(pre_db, post_db->schema);

  result = _handle_db_changes(pre_db, post_db, true);
  cfg_db_remove(pre_db);
  return result;
}

/**
 * AVL comparator for two cfg_schema_entry_key entities.
 * Will compare key.type first, if these are the same it will
 * compare key.entry. NULL is valid as an entry and is smaller
 * than all non-NULL entries. NULL is NOT valid as a type.
 *
 * @param p1 pointer to first key
 * @param p2 pointer to second key
 * @return <0 if p1 comes first, 0 if both are the same, >0 otherwise
 */
int
cfg_avlcmp_schemaentries(const void *p1, const void *p2) {
  const struct cfg_schema_entry_key *key1, *key2;
  int result;

  key1 = p1;
  key2 = p2;

  result = cfg_avlcmp_keys(key1->type, key2->type);
  if (result != 0) {
    return result;
  }

  if (key1->entry == NULL) {
    return key2->entry == NULL ? 0 : -1;
  }
  if (key2->entry == NULL) {
    return 1;
  }

  return cfg_avlcmp_keys(key1->entry, key2->entry);
}

/**
 * Schema entry validator for string maximum length.
 * See CFG_VALIDATE_STRING_LEN macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param section_name name of section type and name
 * @param value value of schema entry, NULL for help text generation
 * @param out pointer to autobuffer for validator output
 * @return 0 if validation found no problems, -1 otherwise
 */
int
cfg_schema_validate_strlen(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  if (strlen(value) > entry->validate_param[0].s) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s is longer than %"PRINTF_SIZE_T_SPECIFIER" characters",
        value, entry->key.entry, section_name, entry->validate_param[0].s);
    return -1;
  }
  return 0;
}

/**
 * Schema entry validator for strings printable characters
 * and a maximum length.
 * See CFG_VALIDATE_PRINTABLE() and CFG_VALIDATE_PRINTABLE_LEN()
 * macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param section_name name of section type and name
 * @param value value of schema entry
 * @param out pointer to autobuffer for validator output
 * @return 0 if validation found no problems, -1 otherwise
 */
int
cfg_schema_validate_printable(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  if (cfg_schema_validate_strlen(entry, section_name, value, out)) {
    return 1;
  }
  if (!str_is_printable(value)) {
    /* not a printable ascii character */
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s has has non-printable characters",
        value, entry->key.entry, section_name);
    return 1;

  }
  return 0;
}

/**
 * Schema entry validator for choice (list of possible strings)
 * List selection will be case insensitive.
 * See CFG_VALIDATE_CHOICE() macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param section_name name of section type and name
 * @param value value of schema entry
 * @param out pointer to autobuffer for validator output
 * @return 0 if validation found no problems, -1 otherwise
 */
int
cfg_schema_validate_choice(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  const char **list = entry->validate_param[0].ptr;
  int i;

  i = cfg_get_choice_index(value, list, entry->validate_param[1].s);
  if (i >= 0) {
    return 0;
  }

  cfg_append_printable_line(out, "Unknown value '%s'"
      " for entry '%s' in section %s",
      value, entry->key.entry, section_name);
  return -1;
}

/**
 * Schema entry validator for integers.
 * See CFG_VALIDATE_INT() and CFG_VALIDATE_INT_MINMAX() macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param section_name name of section type and name
 * @param value value of schema entry
 * @param out pointer to autobuffer for validator output
 * @return 0 if validation found no problems, -1 otherwise
 */
int
cfg_schema_validate_int(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  int32_t i;
  char *endptr = NULL;

  i = strtol(value, &endptr, 10);
  if (endptr == NULL || *endptr != 0) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s is not an integer",
        value, entry->key.entry, section_name);
    return 1;
  }
  if (i < entry->validate_param[0].i32[0] || i > entry->validate_param[0].i32[1]) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s' in section %s is "
        "not between %d and %d",
        value, entry->key.entry, section_name,
        entry->validate_param[0].i32[0], entry->validate_param[0].i32[1]);
    return 1;
  }
  return 0;
}

/**
 * Schema entry validator for fractional integers.
 * See CFG_VALIDATE_FRACTIONAL() and CFG_VALIDATE_FRACTIONAL_MINMAX() macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param section_name name of section type and name
 * @param value value of schema entry
 * @param out pointer to autobuffer for validator output
 * @return 0 if validation found no problems, -1 otherwise
 */
int
cfg_schema_validate_fractional(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  int64_t i;

  if (cfg_fraction_from_string(&i, value, entry->validate_param[1].i32[0])) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s is not a fractional integer"
        " with a maximum of %d fractional digits",
        value, entry->key.entry, section_name, entry->validate_param[1].i32[0]);
    return 1;
  }
  if (i < entry->validate_param[0].i32[0] || i > entry->validate_param[0].i32[1]) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s' in section %s is "
        "not between %d and %d",
        value, entry->key.entry, section_name,
        entry->validate_param[0].i32[0], entry->validate_param[0].i32[1]);
    return 1;
  }
  return 0;
}

/**
 * Schema entry validator for network addresses and prefixes.
 * See CFG_VALIDATE_NETADDR_*() macros in cfg_schema.h
 * @param entry pointer to schema entry
 * @param section_name name of section type and name
 * @param value value of schema entry
 * @param out pointer to autobuffer for validator output
 * @return 0 if validation found no problems, -1 otherwise
 */
int
cfg_schema_validate_netaddr(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  struct netaddr addr;
  uint8_t max_prefix;
  int i;

  if (_get_known_prefix(&addr, value)) {
    if (netaddr_from_string(&addr, value)) {
      cfg_append_printable_line(out, "Value '%s' for entry '%s'"
          " in section %s is no valid network address",
          value, entry->key.entry, section_name);
      return -1;
    }
  }

  max_prefix = netaddr_get_maxprefix(&addr);

  /* check prefix length */
  if (netaddr_get_prefix_length(&addr) > max_prefix) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s has an illegal prefix length",
        value, entry->key.entry, section_name);
    return -1;
  }
  if (!entry->validate_param[1].b && netaddr_get_prefix_length(&addr) != max_prefix) {
    cfg_append_printable_line(out, "Value '%s' for entry '%s'"
        " in section %s must be a single address, not a prefix",
        value, entry->key.entry, section_name);
    return -1;
  }

  for (i=0; i<5; i++) {
    int8_t type;

    type = entry->validate_param[0].i8[i];
    if (type == netaddr_get_address_family(&addr)) {
      return 0;
    }
  }

  /* at least one condition was set, but no one matched */
  cfg_append_printable_line(out, "Value '%s' for entry '%s'"
      " in section '%s' is wrong address type",
      value, entry->key.entry, section_name);
  return -1;
}

/**
 * Help generator for string maximum length validator.
 * See CFG_VALIDATE_STRING_LEN macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param out pointer to autobuffer for help output
 */
void
cfg_schema_help_strlen(
    const struct cfg_schema_entry *entry, struct autobuf *out) {
  cfg_append_printable_line(out, "    Parameter must have a maximum"
      " length of %"PRINTF_SIZE_T_SPECIFIER" characters",
      entry->validate_param[0].s);
}

/**
 * Help generator for strings printable characters
 * and a maximum length validator.
 * See CFG_VALIDATE_PRINTABLE() and CFG_VALIDATE_PRINTABLE_LEN()
 * macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param out pointer to autobuffer for validator output
 */
void
cfg_schema_help_printable(
    const struct cfg_schema_entry *entry, struct autobuf *out) {
  cfg_schema_help_printable(entry, out);
  cfg_append_printable_line(out, "    Parameter must only contain printable characters.");
}

/**
 * Help generator for choice (list of possible strings) validator
 * List selection will be case insensitive.
 * See CFG_VALIDATE_CHOICE() macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param out pointer to autobuffer for validator output
 */
void
cfg_schema_help_choice(
    const struct cfg_schema_entry *entry, struct autobuf *out) {
  const char **list = entry->validate_param[0].ptr;
  size_t i;

  cfg_append_printable_line(out, "    Parameter must be on of the following list:");

  abuf_puts(out, "    ");
  for (i=0; i < entry->validate_param[1].s; i++) {
    abuf_appendf(out, "%s'%s'",
        i==0 ? "" : ", ", list[i]);
  }
  abuf_puts(out, "\n");
}

/**
 * Help generator for integer validator.
 * See CFG_VALIDATE_INT() and CFG_VALIDATE_INT_MINMAX() macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param out pointer to autobuffer for validator output
 */
void
cfg_schema_help_int(
    const struct cfg_schema_entry *entry, struct autobuf *out) {
  if (entry->validate_param[0].i32[0] > INT32_MIN) {
    if (entry->validate_param[0].i32[1] < INT32_MAX) {
      cfg_append_printable_line(out, "    Parameter must be an integer between %d and %d",
          entry->validate_param[0].i32[0], entry->validate_param[0].i32[1]);
    }
    else {
      cfg_append_printable_line(out, "    Parameter must be an integer larger or equal than %d",
          entry->validate_param[0].i32[0]);
    }
  }
  else {
    if (entry->validate_param[0].i32[1] < INT32_MAX) {
      cfg_append_printable_line(out, "    Parameter must be an integer less or equal than %d",
          entry->validate_param[0].i32[1]);
    }
    else {
      cfg_append_printable_line(out, "    Parameter must be a 32 bit signed integer");
    }
  }
}

/**
 * Help generator for integer validator.
 * See CFG_VALIDATE_INT() and CFG_VALIDATE_INT_MINMAX() macro in cfg_schema.h
 * @param entry pointer to schema entry
 * @param out pointer to autobuffer for validator output
 */
void
cfg_schema_help_fractional(
    const struct cfg_schema_entry *entry, struct autobuf *out) {
  struct fraction_str buf1, buf2;

  if (entry->validate_param[0].i32[0] > INT32_MIN) {
    if (entry->validate_param[0].i32[1] < INT32_MAX) {
      cfg_append_printable_line(out, "    Parameter must be an number between %s and %s",
          cfg_fraction_to_string(&buf1,
              entry->validate_param[0].i32[0], entry->validate_param[1].i32[0]),
          cfg_fraction_to_string(&buf2,
              entry->validate_param[0].i32[1], entry->validate_param[1].i32[0]));
    }
    else {
      cfg_append_printable_line(out, "    Parameter must be an number larger or equal than %s",
          cfg_fraction_to_string(&buf1,
              entry->validate_param[0].i32[0], entry->validate_param[1].i32[0]));
    }
  }
  else {
    if (entry->validate_param[0].i32[1] < INT32_MAX) {
      cfg_append_printable_line(out, "    Parameter must be an number less or equal than %s",
          cfg_fraction_to_string(&buf2,
              entry->validate_param[0].i32[1], entry->validate_param[1].i32[0]));
    }
    else {
      cfg_append_printable_line(out, "    Parameter must be a signed integer");
    }
  }
}

/**
 * Help generator for network addresses and prefixes validator.
 * See CFG_VALIDATE_NETADDR_*() macros in cfg_schema.h
 * @param entry pointer to schema entry
 * @param out pointer to autobuffer for validator output
 */
void
cfg_schema_help_netaddr(
    const struct cfg_schema_entry *entry, struct autobuf *out) {
  int8_t type;
  bool first;
  int i;

  abuf_puts(out, "    Parameter must be an address of the following type: ");

  first = true;
  for (i=0; i<5; i++) {
    type = entry->validate_param[0].i8[i];

    if (first) {
      first = false;
    }
    else {
      abuf_puts(out, ", ");
    }

    switch (type) {
      case AF_INET:
        abuf_puts(out, "IPv4");
        break;
      case AF_INET6:
        abuf_puts(out, "IPv6");
        break;
      case AF_MAC48:
        abuf_puts(out, "MAC48");
        break;
      case AF_EUI64:
        abuf_puts(out, "EUI64");
        break;
      default:
        abuf_puts(out, "Unspec (-)");
        break;
    }
  }

  if (entry->validate_param[1].b) {
    abuf_puts(out, " (with an optional prefix)");
  }
  abuf_puts(out, "\n");
}

/**
 * Binary converter for string pointers. This validator will
 * allocate additional memory for the string.
 * See CFG_MAP_STRING() and CFG_MAP_STRING_LEN() macro
 * in cfg_schema.h
 * @param s_entry pointer to configuration entry schema.
 * @param value pointer to value of configuration entry.
 * @param reference pointer to binary output buffer.
 * @return 0 if conversion succeeded, -1 otherwise.
 */
int
cfg_schema_tobin_strptr(const struct cfg_schema_entry *s_entry __attribute__((unused)),
    const struct const_strarray *value, void *reference) {
  char **ptr;

  ptr = (char **)reference;
  if (*ptr) {
    free(*ptr);
  }

  *ptr = strdup(strarray_get_first_c(value));
  return *ptr == NULL ? -1 : 0;
}

/**
 * Binary converter for string arrays.
 * See CFG_MAP_STRING_ARRAY() macro in cfg_schema.h
 * @param s_entry pointer to configuration entry schema.
 * @param value pointer to value of configuration entry.
 * @param reference pointer to binary output buffer.
 * @return 0 if conversion succeeded, -1 otherwise.
 */
int
cfg_schema_tobin_strarray(const struct cfg_schema_entry *s_entry,
    const struct const_strarray *value, void *reference) {
  char *ptr;

  ptr = (char *)reference;

  strscpy(ptr, strarray_get_first_c(value), s_entry->validate_param[0].s);
  return 0;
}

/**
 * Binary converter for integers chosen as an index in a predefined
 * string list.
 * See CFG_MAP_CHOICE() macro in cfg_schema.h
 * @param s_entry pointer to configuration entry schema.
 * @param value pointer to value of configuration entry.
 * @param reference pointer to binary output buffer.
 * @return 0 if conversion succeeded, -1 otherwise.
 */
int
cfg_schema_tobin_choice(const struct cfg_schema_entry *s_entry,
    const struct const_strarray *value, void *reference) {
  int *ptr;

  ptr = (int *)reference;

  *ptr = cfg_get_choice_index(strarray_get_first_c(value),
      s_entry->validate_param[0].ptr, s_entry->validate_param[1].s);
  return 0;
}

/**
 * Binary converter for integers.
 * See CFG_MAP_INT() macro in cfg_schema.h
 * @param s_entry pointer to configuration entry schema.
 * @param value pointer to value of configuration entry.
 * @param reference pointer to binary output buffer.
 * @return 0 if conversion succeeded, -1 otherwise.
 */
int
cfg_schema_tobin_int(const struct cfg_schema_entry *s_entry __attribute__((unused)),
    const struct const_strarray *value, void *reference) {
  int *ptr;

  ptr = (int *)reference;

  *ptr = strtol(strarray_get_first_c(value), NULL, 10);
  return 0;
}

/**
 * Binary converter for integers.
 * See CFG_MAP_INT() macro in cfg_schema.h
 * @param s_entry pointer to configuration entry schema.
 * @param value pointer to value of configuration entry.
 * @param reference pointer to binary output buffer.
 * @return 0 if conversion succeeded, -1 otherwise.
 */
int
cfg_schema_tobin_fractional(const struct cfg_schema_entry *s_entry,
    const struct const_strarray *value, void *reference) {
  int *ptr;
  int64_t i, result;

  ptr = (int *)reference;

  result = cfg_fraction_from_string(&i, strarray_get_first_c(value),
      s_entry->validate_param[1].i32[0]);
  if (result == 0) {
    *ptr = i;
  }
  return result;
}

/**
 * Binary converter for netaddr objects.
 * See CFG_MAP_NETADDR_*() macros in cfg_schema.h
 * @param s_entry pointer to configuration entry schema.
 * @param value pointer to value of configuration entry.
 * @param reference pointer to binary output buffer.
 * @return 0 if conversion succeeded, -1 otherwise.
 */int
cfg_schema_tobin_netaddr(const struct cfg_schema_entry *s_entry __attribute__((unused)),
    const struct const_strarray *value, void *reference) {
  struct netaddr *ptr;

  ptr = (struct netaddr *)reference;

  if (!_get_known_prefix(ptr, strarray_get_first_c(value))) {
    return 0;
  }
  return netaddr_from_string(ptr, strarray_get_first_c(value));
}

 /**
  * Binary converter for booleans.
  * See CFG_MAP_BOOL() macro in cfg_schema.h
  * @param s_entry pointer to configuration entry schema.
  * @param value pointer to value of configuration entry.
  * @param reference pointer to binary output buffer.
  * @return 0 if conversion succeeded, -1 otherwise.
  */
int
cfg_schema_tobin_bool(const struct cfg_schema_entry *s_entry __attribute__((unused)),
    const struct const_strarray *value, void *reference) {
  bool *ptr;

  ptr = (bool *)reference;

  *ptr = cfg_get_bool(strarray_get_first_c(value));
  return 0;
}

/**
 * Binary converter for list of strings.
 * See CFG_MAP_STRINGLIST() macro in cfg_schema.h
 * @param s_entry pointer to configuration entry schema.
 * @param value pointer to value of configuration entry.
 * @param reference pointer to binary output buffer.
 * @return 0 if conversion succeeded, -1 otherwise.
 */
int
cfg_schema_tobin_stringlist(const struct cfg_schema_entry *s_entry __attribute__((unused)),
    const struct const_strarray *value, void *reference) {
  struct strarray *array;

  array = (struct strarray *)reference;

  if (!value->value[0]) {
    strarray_init(array);
    return 0;
  }
  return strarray_copy_c(array, value);
}

/**
 * Compare two sets of databases and trigger delta listeners according to connected
 * schema.
 * @param pre_change pre-change database
 * @param post_change post-change database
 * @param startup if true, also trigger unnamed sections which don't change, but are
 *   of type CFG_SSMODE_UNNAMED (and not CFG_SSMODE_UNNAMED_OPTIONAL_STARTUP_TRIGGER).
 * @return -1 if an error happened, 0 otherwise
 */
static int
_handle_db_changes(struct cfg_db *pre_change, struct cfg_db *post_change, bool startup) {
  struct cfg_schema_section *s_section;
  struct cfg_section_type *pre_type, *post_type;
  struct cfg_named_section *pre_named, *post_named, *named_it;

  if (pre_change->schema == NULL || pre_change->schema != post_change->schema) {
    /* no valid schema found */
    return -1;
  }

  avl_for_each_element(&pre_change->schema->handlers, s_section, _delta_node) {
    /* look over all schemas in the order of their delta priority */

    pre_type = cfg_db_find_sectiontype(pre_change, s_section->type);
    post_type = cfg_db_find_sectiontype(post_change, s_section->type);

    if (post_type) {
      /* handle new sections and changes */
      pre_named = NULL;
      CFG_FOR_ALL_SECTION_NAMES(post_type, post_named, named_it) {
        _handle_named_section_change(s_section,
            pre_change, post_change, post_named->name, startup);
      }
    }
    if (pre_type) {
      /* handle removed sections */
      post_named = NULL;
      CFG_FOR_ALL_SECTION_NAMES(pre_type, pre_named, named_it) {
        if (post_type) {
          post_named = cfg_db_get_named_section(post_type, pre_named->name);
        }

        if (!post_named) {
          _handle_named_section_change(s_section,
              pre_change, post_change, pre_named->name, startup);
        }
      }
    }
    if (startup && s_section->mode == CFG_SSMODE_UNNAMED
        && pre_type == NULL && post_type == NULL) {
      _handle_named_section_change(s_section, pre_change, post_change, NULL, true);
    }
  }
  return 0;
}

/**
 * Validates on configuration entry.
 * @param schema_section pointer to schema section
 * @param db pointer to database
 * @param section pointer to database section type
 * @param named pointer to named section
 * @param entry pointer to configuration entry
 * @param section_name name of section including type (for debug output)
 * @param cleanup true if bad _entries should be removed
 * @param out error output buffer
 * @return true if an error happened, false otherwise
 */
static bool
_validate_cfg_entry(struct cfg_db *db, struct cfg_section_type *section,
    struct cfg_named_section *named, struct cfg_entry *entry,
    const char *section_name, bool cleanup, struct autobuf *out) {
  struct cfg_schema_entry *schema_entry, *s_entry_it;
  struct cfg_schema_entry_key key;
  bool warning, do_remove;
  char *ptr1;

  warning = false;
  ptr1 = NULL;

  key.type = section->type;
  key.entry = entry->name;

  avl_for_each_elements_with_key(
      &db->schema->entries, schema_entry, _node, s_entry_it, &key) {

    if (schema_entry == NULL) {
      cfg_append_printable_line(out, "Unknown entry '%s'"
          " for section type '%s'", entry->name, section->type);
      return true;
    }

    if (schema_entry->cb_validate == NULL) {
      continue;
    }

    /* now validate syntax */
    ptr1 = entry->val.value;

    do_remove = false;
    while (ptr1 < entry->val.value + entry->val.length) {
      if (!do_remove && schema_entry->cb_validate(schema_entry, section_name, ptr1, out) != 0) {
        /* warning is generated by the validate callback itself */
        warning = true;
      }

      if ((warning || do_remove) && cleanup) {
        /* illegal entry found, remove it */
        strarray_remove_ext(&entry->val, ptr1, false);
      }
      else {
        ptr1 += strlen(ptr1) + 1;
      }

      if (!schema_entry->list) {
        do_remove = true;
      }
    }

    if (strarray_is_empty(&entry->val)) {
      /* remove entry */
      cfg_db_remove_entry(db, section->type, named->name, entry->name);
    }
  }
  return warning;
}

/**
 * Checks a database section for missing mandatory _entries
 * @param schema_section pointer to schema of section
 * @param db pointer to database
 * @param section pointer to database section type
 * @param named pointer to named section
 * @param section_name name of section including type (for debug output)
 * @param out error output buffer
 * @return true if an error happened, false otherwise
 */
static bool
_check_missing_entries(struct cfg_schema_section *schema_section,
    struct cfg_db *db, struct cfg_named_section *named,
    const char *section_name, struct autobuf *out) {
  struct cfg_schema_entry *first_schema_entry, *schema_entry;
  struct cfg_schema_entry_key key;
  bool warning, error;

  warning = false;
  error = false;

  key.type = schema_section->type;
  key.entry = NULL;

  /* check for missing values */
  first_schema_entry = avl_find_ge_element(&db->schema->entries, &key, schema_entry, _node);
  avl_for_element_to_last(&db->schema->entries, first_schema_entry, schema_entry, _node) {
    if (cfg_cmp_keys(schema_entry->key.type, schema_section->type) != 0)
      break;

    if (!strarray_is_empty_c(&schema_entry->def)) {
      continue;
    }

    /* mandatory parameter */
    warning = !cfg_db_find_entry(db, schema_entry->key.type,
        named->name, schema_entry->key.entry);
    error |= warning;
    if (warning) {
      cfg_append_printable_line(out, "Missing mandatory value for entry '%s' in section %s",
          schema_entry->key.entry, section_name);
    }
  }
  return error;
}

/**
 * Handle changes in a single named section
 * @param s_section schema entry for section
 * @param pre_change pointer to database before changes
 * @param post_change pointer to database after changes
 * @param name
 */
static void
_handle_named_section_change(struct cfg_schema_section *s_section,
    struct cfg_db *pre_change, struct cfg_db *post_change,
    const char *name, bool startup) {
  struct cfg_schema_entry *entry;
  bool changed;
  size_t i;

  if ((s_section->mode == CFG_SSMODE_NAMED
       || s_section->mode == CFG_SSMODE_NAMED_MANDATORY)
      && name == NULL) {
    /*
     * ignore unnamed data entry for named sections, they are only
     * used for delivering defaults
     */
    return;
  }

  s_section->pre = cfg_db_find_namedsection(pre_change, s_section->type, name);
  s_section->post = cfg_db_find_namedsection(post_change, s_section->type, name);

  changed = false;

  for (i=0; i<s_section->_entry_count; i++) {
    entry = &s_section->_entries[i];

    /* read values */
    entry->pre = cfg_db_get_entry_value(
        pre_change, s_section->type, name, entry->key.entry);
    entry->post = cfg_db_get_entry_value(
        post_change, s_section->type, name, entry->key.entry);

    entry->delta_changed = strarray_cmp_c(entry->pre, entry->post);
    changed |= entry->delta_changed;
  }

  if (changed || startup) {
    s_section->section_name = name;
    s_section->cb_delta_handler();
  }
}

/**
 * Lookup if the name is in the list of known prefixes and
 * sets the address if the name is found.
 * Netaddr target will not be touched if name is not in known
 * prefix list.
 * @param dst pointer to target netaddr
 * @param name text name to look for
 * @return 0 if name was found, -1 otherwise
 */
static int
_get_known_prefix(struct netaddr *dst, const char *name) {
  size_t i;

  for (i=0; i<ARRAYSIZE(_known_prefixes); i++) {
    if (strcasecmp(name, _known_prefixes[i].name) == 0) {
      memcpy(dst, _known_prefixes[i].prefix, sizeof(*dst));
      return 0;
    }
  }
  return -1;
}

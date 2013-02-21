
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

#include "common/common_types.h"
#include "common/netaddr.h"

#include "config/cfg_db.h"
#include "config/cfg_schema.h"
#include "config/cfg.h"

#include "core/olsr_netaddr_acl.h"

static int _handle_control_cmd(struct olsr_netaddr_acl *, const char *);
static bool _is_in_array(struct netaddr *, size_t, struct netaddr *);

const char ACL_FIRST_REJECT[13] = "first_reject";
const char ACL_FIRST_ACCEPT[13] = "first_accept";
const char ACL_DEFAULT_ACCEPT[15] = "default_accept";
const char ACL_DEFAULT_REJECT[15] = "default_reject";

/**
 * Initialize an ACL object. It will contain no addresses on both
 * accept and reject list and will be "accept first", "reject default".
 * @param acl pointer to ACL
 */
void
olsr_acl_add(struct olsr_netaddr_acl *acl) {
  memset(acl, 0, sizeof(*acl));
}

void
olsr_acl_remove(struct olsr_netaddr_acl *acl) {
  free(acl->accept);
  free(acl->reject);

  memset(acl, 0, sizeof(*acl));
}

/**
 * Initialize an ACL with a list of string parameters.
 * @param acl pointer to uninitialized ACL
 * @param value pointer to string-array of text arguments for the ACL
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_acl_from_strarray(struct olsr_netaddr_acl *acl,
    const struct const_strarray *value) {
  size_t accept_count, reject_count;
  const char *ptr;
  accept_count = 0;
  reject_count = 0;

  /* clear acl struct */
  memset(acl, 0, sizeof(*acl));

  /* count number of address entries */
  FOR_ALL_STRINGS(value, ptr) {
    if (_handle_control_cmd(acl, ptr) == 0) {
      continue;
    }

    if (ptr[0] == '-') {
      reject_count++;
    }
    else {
      accept_count++;
    }
  }

  /* allocate memory */
  if (accept_count > 0) {
    acl->accept = calloc(accept_count, sizeof(struct netaddr));
    if (acl->accept == NULL) {
      goto from_entry_error;
    }
  }
  if (reject_count > 0) {
    acl->reject = calloc(reject_count, sizeof(struct netaddr));
    if (acl->reject == NULL) {
      goto from_entry_error;
    }
  }

  /* read netaddr strings into buffers */
  FOR_ALL_STRINGS(value, ptr) {
    const char *addr;
    if (_handle_control_cmd(acl, ptr) == 0) {
      continue;
    }

    addr = ptr;
    if (*ptr == '-' || *ptr == '+') {
      addr++;
    }

    if (*ptr == '-') {
      if (netaddr_from_string(&acl->reject[acl->reject_count], addr)) {
        goto from_entry_error;
      }
      acl->reject_count++;
    }
    else {
      if (netaddr_from_string(&acl->accept[acl->accept_count], addr)) {
         goto from_entry_error;
      }
      acl->accept_count++;
    }
  }
  return 0;

from_entry_error:
  olsr_acl_remove(acl);
  return -1;
}

/**
 * Copy one ACL to another one.
 * @param to pointer to destination ACL
 * @param from pointer to source ACL
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_acl_copy(struct olsr_netaddr_acl *to, struct olsr_netaddr_acl *from) {
  olsr_acl_remove(to);
  memcpy(to, from, sizeof(*to));

  if (to->accept_count) {
    to->accept = calloc(to->accept_count, sizeof(struct netaddr));
    if (to->accept == NULL) {
      return -1;
    }
    memcpy(to->accept, from->accept, to->accept_count * sizeof(struct netaddr));
  }

  if (to->reject_count) {
    to->reject = calloc(to->reject_count, sizeof(struct netaddr));
    if (to->reject == NULL) {
      return -1;
    }
    memcpy(to->reject, from->reject, to->reject_count * sizeof(struct netaddr));
  }
  return 0;
}

/**
 * Check if an address is accepted by an ACL
 * @param acl pointer to ACL
 * @param addr pointer to address
 * @return true if accepted, false otherwise
 */
bool
olsr_acl_check_accept(struct olsr_netaddr_acl *acl, struct netaddr *addr) {
  if (acl->reject_first) {
    if (_is_in_array(acl->reject, acl->reject_count, addr)) {
      return false;
    }
  }

  if (_is_in_array(acl->accept, acl->accept_count, addr)) {
    return true;
  }

  if (!acl->reject_first) {
    if (_is_in_array(acl->reject, acl->reject_count, addr)) {
      return false;
    }
  }

  return acl->accept_default;
}

/**
 * Schema entry validator for access control lists.
 * See CFG_VALIDATE_ACL_*() macros.
 * @param entry pointer to schema entry
 * @param section_name name of section type and name
 * @param value value of schema entry
 * @param out pointer to autobuffer for validator output
 * @return 0 if validation found no problems, -1 otherwise
 */
int
olsr_acl_validate(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out) {
  struct olsr_netaddr_acl dummy;

  if (value == NULL) {
    cfg_schema_validate_netaddr(entry, section_name, value, out);
    cfg_append_printable_line(out, "    Additional keywords are %s, %s, %s and %s",
        ACL_FIRST_ACCEPT, ACL_FIRST_REJECT, ACL_DEFAULT_ACCEPT, ACL_DEFAULT_REJECT);
    return 0;
  }

  if (_handle_control_cmd(&dummy, value) == 0) {
    return 0;
  }

  if (*value == '+' || *value == '-') {
    return cfg_schema_validate_netaddr(entry, section_name, value+1, out);
  }
  return cfg_schema_validate_netaddr(entry, section_name, value, out);
}

/**
 * Schema entry binary converter for ACL entries.
 * See CFG_MAP_ACL_*() macros.
 * @param s_entry pointer to schema entry.
 * @param value pointer to value to configuration entry
 * @param reference pointer to binary target
 * @return -1 if an error happened, 0 otherwise
 */
int
olsr_acl_tobin(const struct cfg_schema_entry *s_entry __attribute__((unused)),
    const struct const_strarray *value, void *reference) {
  struct olsr_netaddr_acl *ptr;

  ptr = (struct olsr_netaddr_acl *)reference;

  free(ptr->accept);
  free(ptr->reject);

  return olsr_acl_from_strarray(ptr, value);
}

/**
 * Handle the four control text blocks for ACL initialization.
 * @param acl pointer to ACL
 * @param cmd pointer to control word
 * @return -1 if not a control word, 0 if control world was applied
 */
static int
_handle_control_cmd(struct olsr_netaddr_acl *acl, const char *cmd) {
  if (strcasecmp(cmd, ACL_DEFAULT_ACCEPT) == 0) {
    acl->accept_default = true;
  }
  else if (strcasecmp(cmd, ACL_DEFAULT_REJECT) == 0) {
    acl->accept_default = false;
  }
  else if (strcasecmp(cmd, ACL_FIRST_ACCEPT) == 0) {
    acl->reject_first = false;
  }
  else if (strcasecmp(cmd, ACL_FIRST_REJECT) == 0) {
    acl->reject_first = true;
  }
  else {
    /* no control command, must be an address */
    return -1;
  }

  /* was one of the four valid control commands */
  return 0;
}

/**
 * @param array pointer to array of addresses and networks
 * @param length length of array
 * @param addr pointer of address to be checked
 * @return true if address is inside list of addresses and networks
 */
static bool
_is_in_array(struct netaddr *array, size_t length, struct netaddr *addr) {
  size_t i;

  for (i=0; i<length; i++) {
    if (netaddr_is_in_subnet(&array[i], addr)) {
      return true;
    }
  }
  return false;
}


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

#ifndef OLSR_NETADDR_ACL_H_
#define OLSR_NETADDR_ACL_H_

#include "common/common_types.h"
#include "common/netaddr.h"

#include "config/cfg_schema.h"

#define CFG_VALIDATE_ACL(p_name, p_def, p_help, args...)         _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_acl_validate, .list = true, .validate_param = {{.i8 = {-1}}, {.b = true}}, ##args )
#define CFG_VALIDATE_ACL_HWADDR(p_name, p_def, p_help, args...)  _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_acl_validate, .list = true, .validate_param = {{.i8 = {AF_MAC48, AF_EUI64, -1}}, {.b = true}}, ##args )
#define CFG_VALIDATE_ACL_MAC48(p_name, p_def, p_help, args...)   _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_acl_validate, .list = true, .validate_param = {{.i8 = {AF_MAC48, -1}}, {.b = true}}, ##args )
#define CFG_VALIDATE_ACL_EUI64(p_name, p_def, p_help, args...)   _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_acl_validate, .list = true, .validate_param = {{.i8 = {AF_EUI64, -1}}, {.b = true}}, ##args )
#define CFG_VALIDATE_ACL_V4(p_name, p_def, p_help, args...)      _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_acl_validate, .list = true, .validate_param = {{.i8 = {AF_INET, -1}}, {.b = true}}, ##args )
#define CFG_VALIDATE_ACL_V6(p_name, p_def, p_help, args...)      _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_acl_validate, .list = true, .validate_param = {{.i8 = {AF_INET6, -1}}, {.b = true}}, ##args )
#define CFG_VALIDATE_ACL_V46(p_name, p_def, p_help, args...)     _CFG_VALIDATE(p_name, p_def, p_help, .cb_validate = olsr_acl_validate, .list = true, .validate_param = {{.i8 = {AF_INET, AF_INET6, -1}}, {.b = true}}, ##args )

#define CFG_MAP_ACL(p_reference, p_field, p_name, p_def, p_help, args...)         CFG_VALIDATE_ACL(p_name, p_def, p_help, .cb_to_binary = olsr_acl_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)
#define CFG_MAP_ACL_HWADDR(p_reference, p_field, p_name, p_def, p_help, args...)  CFG_VALIDATE_ACL_HWADDR(p_name, p_def, p_help, .cb_to_binary = olsr_acl_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)
#define CFG_MAP_ACL_MAC48(p_reference, p_field, p_name, p_def, p_help, args...)   CFG_VALIDATE_ACL_MAC48(p_name, p_def, p_help, .cb_to_binary = olsr_acl_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)
#define CFG_MAP_ACL_EUI64(p_reference, p_field, p_name, p_def, p_help, args...)   CFG_VALIDATE_ACL_EUI64(p_name, p_def, p_help, .cb_to_binary = olsr_acl_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)
#define CFG_MAP_ACL_V4(p_reference, p_field, p_name, p_def, p_help, args...)      CFG_VALIDATE_ACL_V4(p_name, p_def, p_help, .cb_to_binary = olsr_acl_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)
#define CFG_MAP_ACL_V6(p_reference, p_field, p_name, p_def, p_help, args...)      CFG_VALIDATE_ACL_V6(p_name, p_def, p_help, .cb_to_binary = olsr_acl_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)
#define CFG_MAP_ACL_V46(p_reference, p_field, p_name, p_def, p_help, args...)     CFG_VALIDATE_ACL_V46(p_name, p_def, p_help, .cb_to_binary = olsr_acl_tobin, .bin_offset = offsetof(struct p_reference, p_field), ##args)

struct olsr_netaddr_acl {
  struct netaddr *accept;
  size_t accept_count;

  struct netaddr *reject;
  size_t reject_count;

  bool reject_first;
  bool accept_default;
};

EXPORT extern const char ACL_FIRST_REJECT[13];
EXPORT extern const char ACL_FIRST_ACCEPT[13];
EXPORT extern const char ACL_DEFAULT_ACCEPT[15];
EXPORT extern const char ACL_DEFAULT_REJECT[15];

EXPORT void olsr_acl_add(struct olsr_netaddr_acl *);
EXPORT int olsr_acl_from_strarray(struct olsr_netaddr_acl *, const struct const_strarray *value);
EXPORT void olsr_acl_remove(struct olsr_netaddr_acl *);
EXPORT int olsr_acl_copy(struct olsr_netaddr_acl *to, struct olsr_netaddr_acl *from);

EXPORT bool olsr_acl_check_accept(struct olsr_netaddr_acl *, struct netaddr *);

EXPORT int olsr_acl_validate(const struct cfg_schema_entry *entry,
    const char *section_name, const char *value, struct autobuf *out);
EXPORT int olsr_acl_tobin(const struct cfg_schema_entry *s_entry,
    const struct const_strarray *value, void *reference);

#endif /* OLSR_NETADDR_ACL_H_ */

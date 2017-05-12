/*
 * Copyright (c) 2016 Cisco and/or its affiliates.
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at:
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
#ifndef included_pneum_h
#define included_pneum_h

#include <stdint.h>
#include <vppinfra/types.h>

#include <vppinfra/byte_order.h>

#define vl_msg_id(n,h) n,
typedef enum
{
  VL_ILLEGAL_MESSAGE_ID = 0,
#include <vpp/api/vpe_all_api_h.h>
#include <vpp_plugins/acl/acl_all_api_h.h>
  VL_MSG_FIRST_AVAILABLE,
} vl_msg_id_t;
#undef vl_msg_id


#define vl_typedefs             /* define message structures */
#include <vpp/api/vpe_all_api_h.h>
#include <vpp_plugins/acl/acl_all_api_h.h>
#undef vl_typedefs

volatile u32 result_ready;
volatile u16 result_msg_id;
//end added

typedef void (*pneum_callback_t)(unsigned char * data, int len);
typedef void (*pneum_error_callback_t)(void *, unsigned char *, int);
int pneum_connect(char * name, char * chroot_prefix, pneum_callback_t cb,
    int rx_qlen);
int pneum_disconnect(void);
int pneum_read(char **data, int *l, unsigned short timeout);
int pneum_write(char *data, int len);
void pneum_free(void * msg);

int pneum_get_msg_index(unsigned char * name);
int pneum_msg_table_size(void);
int pneum_msg_table_max_index(void);

void pneum_rx_suspend (void);
void pneum_rx_resume (void);
void pneum_set_error_handler(pneum_error_callback_t);
//Added on top of original pneum
void* pneum_alloc_msg_size(int nbytes);
int vpp_sync_api(char *request, int request_size, char **reply, int *reply_size);
unsigned int pneum_client_index (void);

#endif

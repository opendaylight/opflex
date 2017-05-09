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

#include <vppinfra/types.h>
#include <vpp-api/pneum/pneum.h>

/**
 * The pneum provides an interface to communicate with VPP using API
 * calls on shared memory between VPP and Opflex Agent.
 * Asynchronous mode:
 *  Client registers a callback. All messages are sent to the callback.
 * Synchronous mode:
 *  Client calls blocking read().
 *  Clients are expected to collate events on a queue.
 *  pneum_write() -> suspends RX thread
 *  pneum_read() -> resumes RX thread
 */

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

typedef void (*pneum_callback_t)(unsigned char * data, int len);

/**
 * Establish a connection to shared memory for API calls from opflex agent
 * to vpp
 */
extern int pneum_connect(char * name, char * chroot_prefix, pneum_callback_t cb,
    int rx_qlen);

/**
 * Terminate the established connection with Shared memory
 */
extern int pneum_disconnect(void);

/**
 * Read the message(s) from shared memory
 */
extern int pneum_read(char **data, int *l, unsigned short timeout);

/**
 * Write the message(s) to shared memory
 */
extern int pneum_write(char *data, int len);

extern int pneum_get_msg_index(unsigned char * name);
extern int pneum_msg_table_size(void);
#endif

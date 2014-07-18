/*
* Copyright (c) 2014 Cisco Systems, Inc. and others.
*               All rights reserved.
*
* This program and the accompanying materials are made available under
* the terms of the Eclipse Public License v1.0 which accompanies this
* distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*/

/*
 * The peovs_modb_interface.c file contains functions which
 * utilize the API of the MODB module. This is the way in which
 * the southbound traffic passes from MODB to the PEOVS.
 *
 * History:
 *     16-July-2014 smann@noironetworks.com = created.
 *
 */
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdbool.h>
#include <stdint.h>

#include "peovs_modb_interface.h"
#include "modb-event.h"
#include "lib/ovs-thread.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(peovs_modb_interface);

static bool pe_modb_iface_pull_quit;
static bool pe_modb_iface_push_quit;
static struct ovs_rwlock pe_modb_iface_pull_rwlock;
static struct ovs_rwlock pe_modb_iface_push_rwlock;

/* static protos */
static bool pe_get_modb_iface_pull_quit(void);
static void peovs_modb_iface_process_pull(modb_event_t *);

/*
 * \brief
 *   peovs_modb_interface_pull() - subscribes to modb-event module for
 *    receipt of events (policies) to be implemented
 *   
 * This function should only be called as an argument to pthread_create()
 *
 * where:
 * @param0 <args> unused pointer to args          - I
 * @param1 <>          - I
 * @returns: 0 if sucessful, else != 0
 *
 */
void *peovs_modb_interface_pull(void *args) {
    (void) args;
    int rval = 0;

    VLOG_ENTER(__func__);

    ovs_rwlock_init(&pe_modb_iface_pull_rwlock);
    pe_set_modb_iface_pull_quit(false);

    if((rval = modb_event_subscribe(MEVT_TYPE_ANY, MEVT_SRC_SOUTH))
        == 0)
    {
        modb_event_t *pull_event;

        while (pe_get_modb_iface_pull_quit() == false)
        {
            if((rval = modb_event_wait(&pull_event)) == 0)
            {
            //once we get an event, push it onto the modb_rb
                peovs_modb_iface_process_pull(pull_event);
            } else {
                VLOG_ERR("modb_event_wait() returned %i\n", rval);
                break;
            }
        }
        /* I don't care about the unsubscribe return code because
         * I'm terminating
         */
        (void) modb_event_unsubscribe(MEVT_TYPE_ANY, MEVT_SRC_SOUTH);
        modb_event_free(pull_event);
    }

    VLOG_LEAVE(__func__);
    pthread_exit((void *) &rval);
}

static void peovs_modb_iface_process_pull(modb_event_t *pull_event)
{

    VLOG_ENTER(__func__);

    //process the list and push into modb_rb
    // this func may not be necessary...

    VLOG_LEAVE(__func__);
}

static bool
pe_get_modb_iface_pull_quit() {
    bool bret;

    ovs_rwlock_rdlock(&pe_modb_iface_pull_rwlock);
    bret = pe_modb_iface_pull_quit;
    ovs_rwlock_unlock(&pe_modb_iface_pull_rwlock);
    return(bret);
}

void
pe_set_modb_iface_pull_quit(bool nval) {

    ovs_rwlock_wrlock(&pe_modb_iface_pull_rwlock);
    pe_modb_iface_pull_quit = nval;
    fprintf(stderr,"New value in %s is %d\n",__func__,pe_modb_iface_pull_quit);
    ovs_rwlock_unlock(&pe_modb_iface_pull_rwlock);

}


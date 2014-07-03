/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */

#define USE_VLOG 1

#include <config.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>

#include "gendcls.h"
#include "coverage.h"
#include "config-file.h"
#include "meo-util.h"
#include "ovs-thread.h"
#include "tv-util.h"
#include "modb.h"
#include "modb-event.h"
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(modb_event);

/* 
 * modb_event.c - This is the MODB eventing sub-system modeule.
 *      When events occur in the MODB and event is generated and if 
 *      a calling program has created an event , a quese is created such
 *      that when the event is detected and someone has subscribed to that
 *      event they are woken up with said event. This eventing system 
 *      is based upon a publish-->subscribe model. Where the queue is created
 *      with what type of events will be published to that queue and 
 *      subscribers subscript to that quese to receive events.

 *
 * History:
 *     28-JUN-2014 dkehn@noironetworks.com - created.
 */

/* 
 * local dcls 
 */
static bool modb_event_initialized = false;
static struct ovs_rwlock subscriber_arr_rwlock;
static int subscriber_count = 0;
static int max_subscribers = 10;
static subscriber_p *subscriber_arr;
static char *gdebug_level;
static char *this_debug_level;

/* 
 * Private protos
 */
static inline bool is_initialized(void) { 
    return(modb_event_initialized); 
}

static int modb_event_find_slot(void);
static int modb_event_find_open_slot(void);
static char *mevt_obj_to_string(int otype);
static int modb_event_send(modb_event_p evtp, size_t evtp_sz);

/* 
 * modb_event_initialize - setup the event subscribe sub-system.
 *
 */
bool modb_event_initialize(void)
{
    static char *mod = "modb_event_initialize";
    bool retb = false;
    char *max_subs;
    subscriber_p sp;
    int i, l1, l2;

    /* setup the debug level for the eventing sub-systems 
     * We are going to make the assumption this if global debug is
     * set, then this won't matter.
     * 
     */
    gdebug_level = conf_get_value("global", "debug_level");    
    this_debug_level = conf_get_value(MODB_SECTION, "event_debug_level");
    
    l1 = vlog_get_level_val(gdebug_level);
    l2 = vlog_get_level_val(this_debug_level);

    if (l2 > l1) {
        vlog_set_levels(vlog_module_from_name("modb_event"), -1, l2);
    }
    ENTER(mod);

    if ((max_subs = conf_get_value(MODB_SECTION, "max_subscribers")) == NULL) {
        VLOG_FATAL("%s: error retreiving max_subscribers", mod);
        retb = true;
        goto rtn_return;
    }
    
    max_subscribers = atoi(max_subs);
    subscriber_count = 0;
    subscriber_arr = xzalloc((sizeof(subscriber_t) * max_subscribers) + 1);
    for (i=0; i < max_subscribers; i++) {
        sp = xzalloc(sizeof(subscriber_t));
        memset(sp, 0, sizeof(subscriber_t));
        subscriber_arr[i] = sp;
    }
    ovs_rwlock_init(&subscriber_arr_rwlock);
    modb_event_initialized = true;
 rtn_return:
    LEAVE(mod);
    return(retb);
}

/* 
 * modb_event_destroy - destroys the eventing system. Each subscriber
 * receive an MEVT_TYPE_DESTORY type event. The meaning is that all 
 * 
 */
int modb_event_destroy(void)
{
    static char *mod = "modb_event_destroy";
    int i;
    subscriber_p sp;
    bool do_delay = false;
    struct timespec ts, tem;

    ENTER(mod);

    if (is_initialized()) {
        /* sleep for .25 secs */
        ts.tv_sec = 1;
        ts.tv_nsec = 0;

        ovs_rwlock_wrlock(&subscriber_arr_rwlock);

        for (i = 0; i < max_subscribers; i ++) {
            sp = subscriber_arr[i];
            if (sp->thread_id) {
                ovs_mutex_lock(&sp->mutex);
                sp->evt_type = MEVT_TYPE_DESTROY;
                sp->evt_counter++;
                sp->evt_source = MEVT_SRC_INTERNAL;
                sp->evt_read = false;
                sp->evtp = NULL;
                sp->evt_new++;
                ovs_mutex_unlock(&sp->mutex);
                pthread_cond_signal(&sp->go);
                do_delay = true;
                VLOG_INFO("%s: signalling for destruction:%lu",
                          mod, sp->thread_id);
            }
        }
        if (do_delay) 
            nanosleep(&ts, &tem);
        
        /* for (done_flag = false, tout = 0; !done_flag || tout < 10; tout++ ) { */
        /*     done_flag = true; */
        /*     for (i = 0; i < max_subscribers; i ++) { */
        /*         sp = subscriber_arr[i]; */
        /*         if (sp->evt_read == false && sp->evt_new) { */
        /*             done_flag = false; */
        /*             nanosleep(&ts, &tem); */
        /*             break; */
        /*         } */
        /*     } */
        /* } */
        for (i=0; i < max_subscribers; i++)
            free(subscriber_arr[i]);

        ovs_rwlock_destroy(&subscriber_arr_rwlock);
        free(subscriber_arr);
    } 
    else {
        VLOG_ERR("%s: MODB Events is not initialized", mod);
    }
    LEAVE(mod);
    return(0);
}

/* 
 * modb_event_subscribe - subscribe to the MODB events systems, the events 
 *   you are interested in are defined by the evt_type and evt_source.
 *
 * where:
 *  @param0 <evt_type>           - I
 *          this is a flag of possbile events see MEVT_TYPE_XXXX in 
 *          modb_event.h for details.
 * @param1 <evt_source>          - I
 *          this is the source of the event, see MEVT_SRC_XXXXXXX
 *          for more details.
 * @returns: 0 if sucessful, else != 0
 *
 */
int modb_event_subscribe(unsigned int evt_type, unsigned int evt_source)
{
    static char *mod = "modb_event_subscribe";
    int retc = 0;
    subscriber_p sp;
    int idx;
    
    ENTER(mod);

    if (is_initialized()) {
        /* get the lock on the subscriber_arr */
        ovs_rwlock_wrlock(&subscriber_arr_rwlock);

        /* this this a refinement of a previous subscribe? */
        idx = modb_event_find_slot();
        if (idx >= 0) {
            /* we are adding to the a current subscriber's filter */
            sp = subscriber_arr[idx];
            sp->evt_type |= evt_type;
            sp->evt_source |= evt_source;
            sp->evt_counter = 0;
        } 
        else {
            if (subscriber_count < max_subscribers) {
                if ((idx = modb_event_find_slot()) < 0) {
                    if ((idx = modb_event_find_open_slot()) < 0) {
                        retc = 1;
                        ovs_rwlock_unlock(&subscriber_arr_rwlock);
                        goto rtn_return;
                    }
                }

                sp = subscriber_arr[idx];
                /* sp = xzalloc(sizeof(subscriber_t)); */
                ovs_mutex_init(&sp->mutex);
                sp->thread_id = pthread_self();
                sp->evt_counter = 0;
                sp->evt_type = evt_type;
                sp->evt_source = evt_source;
                pthread_cond_init(&sp->go, &sp->mutex);
                subscriber_arr[idx] = sp;
                subscriber_count++;
                VLOG_DBG("%s: adding thread_id:%lu to subscribers array",
                         mod, sp->thread_id);
            }
        }
        ovs_rwlock_unlock(&subscriber_arr_rwlock);
    }
    else {
        retc = 1;
        VLOG_ERR("%s: modb_event is not initialized, please modb_event_initialize.",
                 mod);
    }
 rtn_return:
    LEAVE(mod);
    return(retc);
}
                                                 
/* 
 * modb_event_unsubscribe - unsubscribe to the MODB events systems. 
 *   The slot will freed up and available. It used the thread_id as 
 *   the unique identifier so the thread that did the subscribe MUST 
 *   be the one that does the unsubscribe. If doing the unsubscribe
 *   with flags and if they are indicating that they only want to un-
 *   subscribe to a certain type of source of event then this event will
 *   keep happening for the events left after the flags have been applied.
 *   To completely unsubscribe 0 or ANY flag terminate the subscriber.
 *
 * where:
 *  @param0 <evt_type>           - I
 *          this is a flag of possbile events see MEVT_TYPE_XXXX in 
 *          modb_event.h for details.
 * @param1 <evt_source>          - I
 *          this is the source of the event, see MEVT_SRC_XXXXXXX
 *          for more details.
 * @returns: 0 if sucessful, else != 0
 *
 */
int modb_event_unsubscribe(unsigned int evt_type, unsigned int evt_source)
{
    static char *mod = "modb_event_unsubscribe";
    int retc = 0;
    subscriber_p sp;
    int idx;
    
    ENTER(mod);

    if (is_initialized()) {
        /* get the lock on the subscriber_arr */
        ovs_rwlock_wrlock(&subscriber_arr_rwlock);

        idx = modb_event_find_slot();
        
        if (idx >= 0) {            
            sp = subscriber_arr[idx];
            ovs_mutex_lock(&sp->mutex);
            sp->evt_type &= ~evt_type;
            sp->evt_source &= ~evt_source;
            if (sp->evt_type == 0 && sp->evt_source == 0) {
                ovs_mutex_unlock(&sp->mutex);
                ovs_mutex_destroy(&sp->mutex);
                memset(sp, 0, sizeof(subscriber_t));
                subscriber_count--;
                VLOG_DBG("%s: removing thread_id:%lu from subscribers array:%d",
                     mod, pthread_self(), subscriber_count);
            }
            else {
                ovs_mutex_unlock(&sp->mutex);
            }
        } else {
            VLOG_DBG("%s: unknown thread_id:%lu from subscribers array:%d",
                     mod, pthread_self(), subscriber_count);
            retc = 1;
        }
        ovs_rwlock_unlock(&subscriber_arr_rwlock);
    } else {
        retc = 1;
        VLOG_ERR("%s: modb_event is not initialized, please modb_event_initialize.",
                 mod);
    }
    LEAVE(mod);
    return(retc);
}

/*
 * modb_event_wait - this is the part of the eventing structure that when called
 * will perform a wait on the cond in the previous subscriber's subscriber_arr slot
 * until that event is signaled from the MODB.
 *
 * NOTE: this function will us the caller's thread id as to determine the subscriber's
 *       slot in the subscriber_arr.
 *
 */
int modb_event_wait(modb_event_p *evtp)
{
    static char *mod = "modb_event_wait";
    int retc = 0;
    int idx;
    subscriber_p sp;

    ENTER(mod);
    if (is_initialized()) {
        idx = modb_event_find_slot();
        if (idx < 0) {
            VLOG_ERR("%s: no subscriber in array with thread_id:%lu", 
                     mod, pthread_self());
            retc =1;
        } else {
            sp = subscriber_arr[idx];
            ovs_mutex_lock(&sp->mutex);
            sp->evt_read = false;
            ovs_mutex_cond_wait(&sp->go, &sp->mutex);
            *evtp = sp->evtp;
            sp->evt_read = true;
            sp->evt_new--;
            ovs_mutex_unlock(&sp->mutex);
        }
    } else {
        retc = 1;
        VLOG_ERR("%s: modb_event is not initialized, please modb_event_initialize.",
                 mod);
    }

    LEAVE(mod);
    return(retc);
}

/* 
 * modb_event_dump - dumps the subscriber_arr and any events in the evnt_p.
 *
 */
void modb_event_dump(void)
{
    int i, a;
    subscriber_p sp;

    if (is_initialized()) {
        VLOG_INFO("======= Subscriber Dump ==========");
        for (i = 0; i < max_subscribers; i++) {
            sp = subscriber_arr[i];
            VLOG_INFO("  slot[%d] id:%lu", i,sp->thread_id);
            if (sp->thread_id) {
                VLOG_INFO("  evt counter  : %ld", sp->evt_counter);
                VLOG_INFO("  evt_type     : 0x%04x", sp->evt_type);
                VLOG_INFO("  evt_source   : 0x%04x", sp->evt_source);
                VLOG_INFO("  evt_read     : %s", (sp->evt_read?"true":"false"));
                if (sp->evtp) {
                    VLOG_INFO("  event ts: %s", tv_show(sp->evtp->timestamp, 
                                                        true, NULL));
                    VLOG_INFO("  obj      : %s", mevt_obj_to_string(sp->evtp->eobj));
                    VLOG_INFO("  etype    : 0x%04x", sp->evtp->etype);
                    VLOG_INFO("  evt_src  : 0x%04x", sp->evtp->esrc);
                    VLOG_INFO("  dp_count : %d", sp->evtp->dp_count);
                    for (a=0; a < sp->evtp->dp_count; a++) 
                        VLOG_INFO("  dp_arr[%d]: %p", a, sp->evtp->dp_arr[i]);
                }
            }
        }
    }
    return;
}

/* 
 * modb_event_free - this is intended to free an event_t from the a wait once a 
 * thread is finished with the event_t structure.
 *
 * where:
 * @param0 <evtp>                - I
 *         this is the memory pointer to the event_p such tht is can be 
 *         appropriately freed.
 *
 */
void modb_event_free(modb_event_p evp)
{
    static char *mod = "modb_event_free";

    ENTER(mod);
    free(evp->dp_arr);
    free(evp);
    LEAVE(mod);
    return;
}

/* 
 * modb_event_push - mechanism for pushing and event to the subscribers in the
 * array.
 *
 * where:
 * @param0 <evtp>                - I
 *         this is the memory pointer to the event_p such tht is can be 
 *         appropriately freed.
 *
 */
int modb_event_push(unsigned int evt_type, unsigned int evt_source,
                     int obj_type, int dp_count, void *dp)
{
    static char *mod = "modb_event_push";
    int retc = 0;
    modb_event_p evtp;
    size_t evt_sz;

    ENTER(mod);
    switch (obj_type) {
    case MEVT_OBJ_NODE:
    case MEVT_OBJ_TREE:
        evt_sz = sizeof(modb_event_t) + (dp_count * sizeof(node_ele_p));
        evtp = (modb_event_p)xzalloc(evt_sz);
        evtp->eobj = obj_type;
        evtp->etype = evt_type;
        evtp->esrc = evt_source;
        evtp->dp_count = dp_count;
        memcpy(evtp->dp_arr, dp, dp_count* sizeof(node_ele_p));
        evtp->timestamp = tv_tod();
        retc = modb_event_send(evtp, evt_sz);
        modb_event_free(evtp);
        break;
        
    case MEVT_OBJ_PROPERTY:
        VLOG_ERR("%s: MEVT_OBJ_PROPERTY unsupported at this time", 
                 mod);
        retc = -2;
        break;
    default:
        VLOG_ERR("%s: unknown object type: %d", mod, obj_type);
        retc = -1;
        break;
    }

    LEAVE(mod);
    return(retc);
}

/* =========================================================================
 * Private code .
 * ====================================================================== */

/* 
 * modb_event_send - perform the sending of an event to the subscribers 
 * waiting. If nothing is waiting will free the event and return.
 *
 * where:
 * @param0 <evtp>               - I
 *         event point that will be passed to the subscribers that are 
 *         waiting for this type of event.
 * @returns: 0, successful, else something other than 1
 *
 */
static int modb_event_send(modb_event_p evtp, size_t evtp_sz)
{
    static char *mod = "modb_event_push";
    int retc = 0;
    int i;
    subscriber_p sp;
    
    ENTER(mod);

    for (i = 0; i < max_subscribers; i++) {
        ovs_rwlock_wrlock(&subscriber_arr_rwlock);
        sp = subscriber_arr[i];
        if ( sp->thread_id && (sp->evt_type && evtp->etype) && 
            (sp->evt_source && evtp->esrc) ) {
            ovs_mutex_lock(&sp->mutex);
            sp->evtp = xmemdup0(evtp, evtp_sz);
            sp->evt_counter++;
            sp->evt_read = false;
            sp->evt_new++;
            ovs_mutex_unlock(&sp->mutex);
            pthread_cond_signal(&sp->go);
            VLOG_DBG("%s: signaling subscriber:slot[%d]:id:%lu count:%d", 
                     mod, i, sp->thread_id, sp->evt_counter);
        }
        ovs_rwlock_unlock(&subscriber_arr_rwlock);
    }
    
    LEAVE(mod);
    return(retc);
}

/*
 * modb_event_find_slot - this used the callers thread_id to determine
 * the slot in the subscriber_arr of the scubscriber's info, if there isn't
 * a prevoius subscriber with this thread_id a -1 is returned, else the idx 
 * in the subscriber_arr is returned.
 */
static int modb_event_find_slot(void)
{
    static char *mod = "modb_event_find_slot";
    int idx = 0;
    pthread_t this_id = pthread_self();
    
    for (idx = 0; idx < max_subscribers; idx++) {
        if (subscriber_arr[idx]->thread_id != 0) {
            if (this_id == subscriber_arr[idx]->thread_id) {
                break;
            }
        }
    }
    if (idx == max_subscribers) {
        idx = -1;
    }

    LEAVE(mod);
    return(idx);
}

/* 
 * modb_event_find_open_slot - find the next open slot in the subscriber_arr
 * and returns it to the caller.
 */
static int modb_event_find_open_slot(void)
{
    static char *mod = "modb_event_find_open_slot";
    int idx = 0;

    ENTER(mod);
    for (idx = 0; idx < max_subscribers; idx++) {
        if (subscriber_arr[idx]->thread_id == 0) {
            break;
        }
    }
    if (idx >= max_subscribers) {
        idx = -1;
        VLOG_ERR("%s: more subscribers than slots allocated: %d max_slots%d",
                 mod, idx, max_subscribers);
    }
    VLOG_DBG("%s: idx=%d", mod, idx);
    LEAVE(mod);
    return(idx);
}


static char *mevt_obj_to_string(int otype)
{
    static char *mevt_obj_lookup[] = {
        "Node",
        "Tree",
        "Property"
    };
    return(mevt_obj_lookup[otype]);
}
    
    

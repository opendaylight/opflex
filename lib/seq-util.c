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

#include "seq-util.h"
#include "util.h"
#include "vlog.h"
#include "dbug.h"

VLOG_DEFINE_THIS_MODULE(seq_util);


/* sequence_init - initialize a sequence.
 *
 * where:
 *  @param0 <sp>                - I
 *          pointer to the sequence ptr.
 *  @param1 <start_num>         - I
 *          starting number for the sequncer
 *  @param2 <bump_by_value>     - I
 *          each time the sequencer is increments the is what it 
 *          will be incremented by.
 * @returns <retb>                - O
 *          0 upon secess, else != 0
 */
bool 
sequence_init(sequencer_p *sp, uint32_t start_num, uint32_t bump_by_value)
{
    static char *mod = "sequence_init";

    ENTER(mod);
    *sp = xzalloc(sizeof(sequencer_t));
    pag_rwlock_init(&(*sp)->rwlock);
    pag_rwlock_wrlock(&(*sp)->rwlock);
    (*sp)->sequencer = start_num;
    (*sp)->bump_value = bump_by_value;
    pag_rwlock_unlock(&(*sp)->rwlock);
    LEAVE(mod);
    return(0);
}

/* sequence_destroy - destorys the sequence.
 *
 * where:
 *  @param0 <sp>         - I
 *          address of the pointer
 */
void
sequence_destroy(sequencer_p *sp)
{
    static char *mod = "sequence_destroy";

    ENTER(mod);
    pag_rwlock_wrlock(&(*sp)->rwlock);
    pag_rwlock_destroy(&(*sp)->rwlock);
    free(*sp);
    LEAVE(mod);
    return;
}
    
/* sequence_next - bumps the sequncer by the bump_value and returnes 
 *    the number.
 *
 * where:
 *  @param0 <sp>         - I
 *          sequence pointer
 * @returns <rtn_seq>                - O
 *          the new sequencer value
 */
uint32_t sequence_next(sequencer_p sp)
{
    static char *mod = "sequence_next";
    uint32_t rtn_seq = 0;

    ENTER(mod);
    pag_rwlock_wrlock(&sp->rwlock);
    sp->sequencer += sp->bump_value;
    rtn_seq = sp->sequencer;
    pag_rwlock_unlock(&sp->rwlock);
    VLOG_DBG("%s: rtn_seq=%u", mod, rtn_seq);
    LEAVE(mod);
    return(rtn_seq);
}
    
/* sequence_current - returns the current value of the sequncer.
 *
 * where:
 *  @param0 <sp>         - I
 *          sequence pointer
 * @returns <rtn_seq>                - O
 *          the sequencer value
 */
uint32_t
sequence_current(sequencer_p sp)
{
    static char *mod = "sequence_current";
    uint32_t rtn_seq = 0;

    ENTER(mod);
    pag_rwlock_rdlock(&sp->rwlock);
    rtn_seq = sp->sequencer;
    pag_rwlock_unlock(&sp->rwlock);
    LEAVE(mod);
    return(rtn_seq);
}

/* sequence_set - set the sequencer.
 *
 * where:
 * @param0 <sp>         - I
 *          sequence pointer
 * @param1 <num>         - I
 *          number to which the sequencer weill be set.
 * @returns <rtn_seq>                - O
 *          the sequencer value
 */
uint32_t
sequence_set(sequencer_p sp, uint32_t num)
{
    static char *mod = "sequence_set";
    uint32_t rtn_seq = 0;

    ENTER(mod);
    pag_rwlock_wrlock(&sp->rwlock);
    rtn_seq = sp->sequencer = num;
    pag_rwlock_unlock(&sp->rwlock);
    LEAVE(mod);
    return(rtn_seq);
}

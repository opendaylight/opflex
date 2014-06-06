/*
 * Copyright (c) 2014 Cisco Systems, Inc. and others.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
#ifndef SEQ_UTIL_H
#define SEQ_UTIL_H 1

#include <config.h>
#include "pag-thread.h"


#define DEFAULT_SEQ_INC 1
#define DEFAULT_SEQ_START_NUMBER 0

typedef struct _sequencer {
    struct pag_rwlock rwlock;
    uint32_t bump_value;
    uint32_t sequencer;
} sequencer_t, *sequencer_p;


extern bool sequence_init(sequencer_p *sp, uint32_t start_num,
                          uint32_t bump_by_value);
extern void sequence_destroy(sequencer_p *sp);
extern uint32_t sequence_next(sequencer_p sp);
extern uint32_t sequence_current(sequencer_p sp);
extern uint32_t sequence_set(sequencer_p sp, uint32_t num);

#endif  /* end of included ifdef */

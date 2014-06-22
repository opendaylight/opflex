/* Copyright (c) 2014 Cisco Systems, Inc.
 *
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

/*
 * Copyright (c) 2013 Nicira, Inc.
 *
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

#include <config.h>

#include "seq.h"

#include <stdbool.h>

#include "hash.h"
#include "hmap.h"
#include "latch.h"
#include "list.h"
#include "pag-thread.h"
#include "poll-loop.h"

/* A sequence number object. */
struct seq {
    uint64_t value PAG_GUARDED;
    struct hmap waiters PAG_GUARDED; /* Contains 'struct seq_waiter's. */
};

/* A thread waiting on a particular seq. */
struct seq_waiter {
    struct seq *seq PAG_GUARDED;            /* Seq being waited for. */
    struct hmap_node hmap_node PAG_GUARDED; /* In 'seq->waiters'. */
    unsigned int pagthread_id PAG_GUARDED;  /* Key in 'waiters' hmap. */

    struct seq_thread *thread PAG_GUARDED; /* Thread preparing to wait. */
    struct list list_node PAG_GUARDED;     /* In 'thread->waiters'. */

    uint64_t value PAG_GUARDED; /* seq->value we're waiting to change. */
};

/* A thread that might be waiting on one or more seqs. */
struct seq_thread {
    struct list waiters PAG_GUARDED; /* Contains 'struct seq_waiter's. */
    struct latch latch PAG_GUARDED;  /* Wakeup latch for this thread. */
    bool waiting PAG_GUARDED;        /* True if latch_wait() already called. */
};

static struct pag_mutex seq_mutex = PAG_MUTEX_INITIALIZER;

static uint64_t seq_next PAG_GUARDED_BY(seq_mutex) = 1;

static pthread_key_t seq_thread_key;

static void seq_init(void);
static struct seq_thread *seq_thread_get(void) PAG_REQUIRES(seq_mutex);
static void seq_thread_exit(void *thread_) PAG_EXCLUDED(seq_mutex);
static void seq_thread_woke(struct seq_thread *) PAG_REQUIRES(seq_mutex);
static void seq_waiter_destroy(struct seq_waiter *) PAG_REQUIRES(seq_mutex);
static void seq_wake_waiters(struct seq *) PAG_REQUIRES(seq_mutex);

/* Creates and returns a new 'seq' object. */
struct seq * PAG_EXCLUDED(seq_mutex)
seq_create(void)
{
    struct seq *seq;

    seq_init();

    seq = xmalloc(sizeof *seq);
    pag_mutex_lock(&seq_mutex);
    seq->value = seq_next++;
    hmap_init(&seq->waiters);
    pag_mutex_unlock(&seq_mutex);

    return seq;
}

/* Destroys 'seq', waking up threads that were waiting on it, if any. */
void
seq_destroy(struct seq *seq)
     PAG_EXCLUDED(seq_mutex)
{
    pag_mutex_lock(&seq_mutex);
    seq_wake_waiters(seq);
    hmap_destroy(&seq->waiters);
    free(seq);
    pag_mutex_unlock(&seq_mutex);
}

/* Increments 'seq''s sequence number, waking up any threads that are waiting
 * on 'seq'. */
void
seq_change(struct seq *seq)
    PAG_EXCLUDED(seq_mutex)
{
    pag_mutex_lock(&seq_mutex);
    seq->value = seq_next++;
    seq_wake_waiters(seq);
    pag_mutex_unlock(&seq_mutex);
}

/* Returns 'seq''s current sequence number (which could change immediately).
 *
 * seq_read() and seq_wait() can be used together to yield a race-free wakeup
 * when an object changes, even without an ability to lock the object.  See
 * Usage in seq.h for details. */
uint64_t
seq_read(const struct seq *seq)
    PAG_EXCLUDED(seq_mutex)
{
    uint64_t value;

    pag_mutex_lock(&seq_mutex);
    value = seq->value;
    pag_mutex_unlock(&seq_mutex);

    return value;
}

static void
seq_wait__(struct seq *seq, uint64_t value)
    PAG_REQUIRES(seq_mutex)
{
    unsigned int id = pagthread_id_self();
    uint32_t hash = hash_int(id, 0);
    struct seq_waiter *waiter;

    HMAP_FOR_EACH_IN_BUCKET (waiter, hmap_node, hash, &seq->waiters) {
        if (waiter->pagthread_id == id) {
            if (waiter->value != value) {
                /* The current value is different from the value we've already
                 * waited for, */
                poll_immediate_wake();
            } else {
                /* Already waiting on 'value', nothing more to do. */
            }
            return;
        }
    }

    waiter = xmalloc(sizeof *waiter);
    waiter->seq = seq;
    hmap_insert(&seq->waiters, &waiter->hmap_node, hash);
    waiter->pagthread_id = id;
    waiter->value = value;
    waiter->thread = seq_thread_get();
    list_push_back(&waiter->thread->waiters, &waiter->list_node);

    if (!waiter->thread->waiting) {
        latch_wait(&waiter->thread->latch);
        waiter->thread->waiting = true;
    }
}

/* Causes the following poll_block() to wake up when 'seq''s sequence number
 * changes from 'value'.  (If 'seq''s sequence number isn't 'value', then
 * poll_block() won't block at all.)
 *
 * seq_read() and seq_wait() can be used together to yield a race-free wakeup
 * when an object changes, even without an ability to lock the object.  See
 * Usage in seq.h for details. */
void
seq_wait(const struct seq *seq_, uint64_t value)
    PAG_EXCLUDED(seq_mutex)
{
    struct seq *seq = CONST_CAST(struct seq *, seq_);

    pag_mutex_lock(&seq_mutex);
    if (value == seq->value) {
        seq_wait__(seq, value);
    } else {
        poll_immediate_wake();
    }
    pag_mutex_unlock(&seq_mutex);
}

/* Called by poll_block() just before it returns, this function destroys any
 * seq_waiter objects associated with the current thread. */
void
seq_woke(void)
    PAG_EXCLUDED(seq_mutex)
{
    struct seq_thread *thread;

    seq_init();

    thread = pthread_getspecific(seq_thread_key);
    if (thread) {
        pag_mutex_lock(&seq_mutex);
        seq_thread_woke(thread);
        thread->waiting = false;
        pag_mutex_unlock(&seq_mutex);
    }
}

static void
seq_init(void)
{
    static struct pagthread_once once = PAGTHREAD_ONCE_INITIALIZER;

    if (pagthread_once_start(&once)) {
        xpthread_key_create(&seq_thread_key, seq_thread_exit);
        pagthread_once_done(&once);
    }
}

static struct seq_thread *
seq_thread_get(void)
    PAG_REQUIRES(seq_mutex)
{
    struct seq_thread *thread = pthread_getspecific(seq_thread_key);
    if (!thread) {
        thread = xmalloc(sizeof *thread);
        list_init(&thread->waiters);
        latch_init(&thread->latch);
        thread->waiting = false;

        xpthread_setspecific(seq_thread_key, thread);
    }
    return thread;
}

static void
seq_thread_exit(void *thread_)
    PAG_EXCLUDED(seq_mutex)
{
    struct seq_thread *thread = thread_;

    pag_mutex_lock(&seq_mutex);
    seq_thread_woke(thread);
    latch_destroy(&thread->latch);
    free(thread);
    pag_mutex_unlock(&seq_mutex);
}

static void
seq_thread_woke(struct seq_thread *thread)
    PAG_REQUIRES(seq_mutex)
{
    struct seq_waiter *waiter, *next_waiter;

    LIST_FOR_EACH_SAFE (waiter, next_waiter, list_node, &thread->waiters) {
        pag_assert(waiter->thread == thread);
        seq_waiter_destroy(waiter);
    }
    latch_poll(&thread->latch);
}

static void
seq_waiter_destroy(struct seq_waiter *waiter)
    PAG_REQUIRES(seq_mutex)
{
    hmap_remove(&waiter->seq->waiters, &waiter->hmap_node);
    list_remove(&waiter->list_node);
    free(waiter);
}

static void
seq_wake_waiters(struct seq *seq)
    PAG_REQUIRES(seq_mutex)
{
    struct seq_waiter *waiter, *next_waiter;

    HMAP_FOR_EACH_SAFE (waiter, next_waiter, hmap_node, &seq->waiters) {
        latch_set(&waiter->thread->latch);
        seq_waiter_destroy(waiter);
    }
}

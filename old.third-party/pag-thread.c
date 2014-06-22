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
 * Copyright (c) 2013, 2014 Nicira, Inc.
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
#include "pag-thread.h"
#include <errno.h>
#include <poll.h>
#include <stdlib.h>
#include <unistd.h>

#include "compiler.h"
#include "hash.h"
#include "poll-loop.h"
#include "socket-util.h"
#include "util.h"

#ifdef __CHECKER__
/* Omit the definitions in this file because they are somewhat difficult to
 * write without prompting "sparse" complaints, without ugliness or
 * cut-and-paste.  Since "sparse" is just a checker, not a compiler, it
 * doesn't matter that we don't define them. */
#else
#include "vlog.h"

VLOG_DEFINE_THIS_MODULE(pag_thread);

/* If there is a reason that we cannot fork anymore (unless the fork will be
 * immediately followed by an exec), then this points to a string that
 * explains why. */
static const char *must_not_fork;

/* True if we created any threads beyond the main initial thread. */
static bool multithreaded;

#define LOCK_FUNCTION(TYPE, FUN) \
    void \
    pag_##TYPE##_##FUN##_at(const struct pag_##TYPE *l_, \
                            const char *where) \
        PAG_NO_THREAD_SAFETY_ANALYSIS \
    { \
        struct pag_##TYPE *l = CONST_CAST(struct pag_##TYPE *, l_); \
        int error = pthread_##TYPE##_##FUN(&l->lock); \
        if (PAG_UNLIKELY(error)) { \
            pag_abort(error, "pthread_%s_%s failed", #TYPE, #FUN); \
        } \
        l->where = where; \
    }
LOCK_FUNCTION(mutex, lock);
LOCK_FUNCTION(rwlock, rdlock);
LOCK_FUNCTION(rwlock, wrlock);

#define TRY_LOCK_FUNCTION(TYPE, FUN) \
    int \
    pag_##TYPE##_##FUN##_at(const struct pag_##TYPE *l_, \
                            const char *where) \
        PAG_NO_THREAD_SAFETY_ANALYSIS \
    { \
        struct pag_##TYPE *l = CONST_CAST(struct pag_##TYPE *, l_); \
        int error = pthread_##TYPE##_##FUN(&l->lock); \
        if (PAG_UNLIKELY(error) && error != EBUSY) { \
            pag_abort(error, "pthread_%s_%s failed", #TYPE, #FUN); \
        } \
        if (!error) { \
            l->where = where; \
        } \
        return error; \
    }
TRY_LOCK_FUNCTION(mutex, trylock);
TRY_LOCK_FUNCTION(rwlock, tryrdlock);
TRY_LOCK_FUNCTION(rwlock, trywrlock);

#define UNLOCK_FUNCTION(TYPE, FUN) \
    void \
    pag_##TYPE##_##FUN(const struct pag_##TYPE *l_) \
        PAG_NO_THREAD_SAFETY_ANALYSIS \
    { \
        struct pag_##TYPE *l = CONST_CAST(struct pag_##TYPE *, l_); \
        int error; \
        l->where = NULL; \
        error = pthread_##TYPE##_##FUN(&l->lock); \
        if (PAG_UNLIKELY(error)) { \
            pag_abort(error, "pthread_%s_%sfailed", #TYPE, #FUN); \
        } \
    }
UNLOCK_FUNCTION(mutex, unlock);
UNLOCK_FUNCTION(mutex, destroy);
UNLOCK_FUNCTION(rwlock, unlock);
UNLOCK_FUNCTION(rwlock, destroy);

#define XPTHREAD_FUNC1(FUNCTION, PARAM1)                \
    void                                                \
    x##FUNCTION(PARAM1 arg1)                            \
    {                                                   \
        int error = FUNCTION(arg1);                     \
        if (PAG_UNLIKELY(error)) {                      \
            pag_abort(error, "%s failed", #FUNCTION);   \
        }                                               \
    }
#define XPTHREAD_FUNC2(FUNCTION, PARAM1, PARAM2)        \
    void                                                \
    x##FUNCTION(PARAM1 arg1, PARAM2 arg2)               \
    {                                                   \
        int error = FUNCTION(arg1, arg2);               \
        if (PAG_UNLIKELY(error)) {                      \
            pag_abort(error, "%s failed", #FUNCTION);   \
        }                                               \
    }

XPTHREAD_FUNC1(pthread_mutex_lock, pthread_mutex_t *);
XPTHREAD_FUNC1(pthread_mutex_unlock, pthread_mutex_t *);
XPTHREAD_FUNC1(pthread_mutexattr_init, pthread_mutexattr_t *);
XPTHREAD_FUNC1(pthread_mutexattr_destroy, pthread_mutexattr_t *);
XPTHREAD_FUNC2(pthread_mutexattr_settype, pthread_mutexattr_t *, int);
XPTHREAD_FUNC2(pthread_mutexattr_gettype, pthread_mutexattr_t *, int *);

XPTHREAD_FUNC1(pthread_rwlockattr_init, pthread_rwlockattr_t *);
XPTHREAD_FUNC1(pthread_rwlockattr_destroy, pthread_rwlockattr_t *);
#ifdef PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP
XPTHREAD_FUNC2(pthread_rwlockattr_setkind_np, pthread_rwlockattr_t *, int);
#endif

XPTHREAD_FUNC2(pthread_cond_init, pthread_cond_t *, pthread_condattr_t *);
XPTHREAD_FUNC1(pthread_cond_destroy, pthread_cond_t *);
XPTHREAD_FUNC1(pthread_cond_signal, pthread_cond_t *);
XPTHREAD_FUNC1(pthread_cond_broadcast, pthread_cond_t *);

XPTHREAD_FUNC2(pthread_join, pthread_t, void **);

typedef void destructor_func(void *);
XPTHREAD_FUNC2(pthread_key_create, pthread_key_t *, destructor_func *);
XPTHREAD_FUNC1(pthread_key_delete, pthread_key_t);
XPTHREAD_FUNC2(pthread_setspecific, pthread_key_t, const void *);

static void
pag_mutex_init__(const struct pag_mutex *l_, int type)
{
    struct pag_mutex *l = CONST_CAST(struct pag_mutex *, l_);
    pthread_mutexattr_t attr;
    int error;

    l->where = NULL;
    xpthread_mutexattr_init(&attr);
    xpthread_mutexattr_settype(&attr, type);
    error = pthread_mutex_init(&l->lock, &attr);
    if (PAG_UNLIKELY(error)) {
        pag_abort(error, "pthread_mutex_init failed");
    }
    xpthread_mutexattr_destroy(&attr);
}

/* Initializes 'mutex' as a normal (non-recursive) mutex. */
void
pag_mutex_init(const struct pag_mutex *mutex)
{
    pag_mutex_init__(mutex, PTHREAD_MUTEX_ERRORCHECK);
}

/* Initializes 'mutex' as a recursive mutex. */
void
pag_mutex_init_recursive(const struct pag_mutex *mutex)
{
    pag_mutex_init__(mutex, PTHREAD_MUTEX_RECURSIVE);
}

/* Initializes 'mutex' as a recursive mutex. */
void
pag_mutex_init_adaptive(const struct pag_mutex *mutex)
{
#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
    pag_mutex_init__(mutex, PTHREAD_MUTEX_ADAPTIVE_NP);
#else
    pag_mutex_init(mutex);
#endif
}

void
pag_rwlock_init(const struct pag_rwlock *l_)
{
    struct pag_rwlock *l = CONST_CAST(struct pag_rwlock *, l_);
    pthread_rwlockattr_t attr;
    int error;

    l->where = NULL;

    xpthread_rwlockattr_init(&attr);
#ifdef PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP
    xpthread_rwlockattr_setkind_np(
        &attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP);
#endif
    error = pthread_rwlock_init(&l->lock, NULL);
    if (PAG_UNLIKELY(error)) {
        pag_abort(error, "pthread_rwlock_init failed");
    }
    xpthread_rwlockattr_destroy(&attr);
}

void
pag_mutex_cond_wait(pthread_cond_t *cond, const struct pag_mutex *mutex_)
{
    struct pag_mutex *mutex = CONST_CAST(struct pag_mutex *, mutex_);
    int error = pthread_cond_wait(cond, &mutex->lock);
    if (PAG_UNLIKELY(error)) {
        pag_abort(error, "pthread_cond_wait failed");
    }
}

DEFINE_EXTERN_PER_THREAD_DATA(pagthread_id, 0);

struct pagthread_aux {
    void *(*start)(void *);
    void *arg;
};

static void *
pagthread_wrapper(void *aux_)
{
    static atomic_uint next_id = ATOMIC_VAR_INIT(1);

    struct pagthread_aux *auxp = aux_;
    struct pagthread_aux aux;
    unsigned int id;

    atomic_add(&next_id, 1, &id);
    *pagthread_id_get() = id;

    aux = *auxp;
    free(auxp);

    return aux.start(aux.arg);
}

void
xpthread_create(pthread_t *threadp, pthread_attr_t *attr,
                void *(*start)(void *), void *arg)
{
    struct pagthread_aux *aux;
    pthread_t thread;
    int error;

    forbid_forking("multiple threads exist");
    multithreaded = true;

    aux = xmalloc(sizeof *aux);
    aux->start = start;
    aux->arg = arg;

    error = pthread_create(threadp ? threadp : &thread, attr,
                           pagthread_wrapper, aux);
    if (error) {
        pag_abort(error, "pthread_create failed");
    }
}

bool
pagthread_once_start__(struct pagthread_once *once)
{
    pag_mutex_lock(&once->mutex);
    if (!pagthread_once_is_done__(once)) {
        return false;
    }
    pag_mutex_unlock(&once->mutex);
    return true;
}

void
pagthread_once_done(struct pagthread_once *once)
{
    atomic_store(&once->done, true);
    pag_mutex_unlock(&once->mutex);
}

/* Asserts that the process has not yet created any threads (beyond the initial
 * thread).
 *
 * ('where' is used in logging.  Commonly one would use
 * assert_single_threaded() to automatically provide the caller's source file
 * and line number for 'where'.) */
void
assert_single_threaded_at(const char *where)
{
    if (multithreaded) {
        VLOG_FATAL("%s: attempted operation not allowed when multithreaded",
                   where);
    }
}

/* Forks the current process (checking that this is allowed).  Aborts with
 * VLOG_FATAL if fork() returns an error, and otherwise returns the value
 * returned by fork().
 *
 * ('where' is used in logging.  Commonly one would use xfork() to
 * automatically provide the caller's source file and line number for
 * 'where'.) */
pid_t
xfork_at(const char *where)
{
    pid_t pid;

    if (must_not_fork) {
        VLOG_FATAL("%s: attempted to fork but forking not allowed (%s)",
                   where, must_not_fork);
    }

    pid = fork();
    if (pid < 0) {
        VLOG_FATAL("%s: fork failed (%s)", where, pag_strerror(errno));
    }
    return pid;
}

/* Notes that the process must not call fork() from now on, for the specified
 * 'reason'.  (The process may still fork() if it execs itself immediately
 * afterward.) */
void
forbid_forking(const char *reason)
{
    pag_assert(reason != NULL);
    must_not_fork = reason;
}

/* Returns true if the process is allowed to fork, false otherwise. */
bool
may_fork(void)
{
    return !must_not_fork;
}

/* pagthread_counter.
 *
 * We implement the counter as an array of N_COUNTERS individual counters, each
 * with its own lock.  Each thread uses one of the counters chosen based on a
 * hash of the thread's ID, the idea being that, statistically, different
 * threads will tend to use different counters and therefore avoid
 * interfering with each other.
 *
 * Undoubtedly, better implementations are possible. */

/* Basic counter structure. */
struct pagthread_counter__ {
    struct pag_mutex mutex;
    unsigned long long int value;
};

/* Pad the basic counter structure to 64 bytes to avoid cache line
 * interference. */
struct pagthread_counter {
    struct pagthread_counter__ c;
    char pad[ROUND_UP(sizeof(struct pagthread_counter__), 64)
             - sizeof(struct pagthread_counter__)];
};

#define N_COUNTERS 16

struct pagthread_counter *
pagthread_counter_create(void)
{
    struct pagthread_counter *c;
    int i;

    c = xmalloc(N_COUNTERS * sizeof *c);
    for (i = 0; i < N_COUNTERS; i++) {
        pag_mutex_init(&c[i].c.mutex);
        c[i].c.value = 0;
    }
    return c;
}

void
pagthread_counter_destroy(struct pagthread_counter *c)
{
    if (c) {
        int i;

        for (i = 0; i < N_COUNTERS; i++) {
            pag_mutex_destroy(&c[i].c.mutex);
        }
        free(c);
    }
}

void
pagthread_counter_inc(struct pagthread_counter *c, unsigned long long int n)
{
    c = &c[hash_int(pagthread_id_self(), 0) % N_COUNTERS];

    pag_mutex_lock(&c->c.mutex);
    c->c.value += n;
    pag_mutex_unlock(&c->c.mutex);
}

unsigned long long int
pagthread_counter_read(const struct pagthread_counter *c)
{
    unsigned long long int sum;
    int i;

    sum = 0;
    for (i = 0; i < N_COUNTERS; i++) {
        pag_mutex_lock(&c[i].c.mutex);
        sum += c[i].c.value;
        pag_mutex_unlock(&c[i].c.mutex);
    }
    return sum;
}

/* Parses /proc/cpuinfo for the total number of physical cores on this system
 * across all CPU packages, not counting hyper-threads.
 *
 * Sets *n_cores to the total number of cores on this system, or 0 if the
 * number cannot be determined. */
static void
parse_cpuinfo(long int *n_cores)
{
    static const char file_name[] = "/proc/cpuinfo";
    char line[128];
    uint64_t cpu = 0; /* Support up to 64 CPU packages on a single system. */
    long int cores = 0;
    FILE *stream;

    stream = fopen(file_name, "r");
    if (!stream) {
        VLOG_DBG("%s: open failed (%s)", file_name, pag_strerror(errno));
        return;
    }

    while (fgets(line, sizeof line, stream)) {
        unsigned int id;

        /* Find the next CPU package. */
        if (pag_scan(line, "physical id%*[^:]: %u", &id)) {
            if (id > 63) {
                VLOG_WARN("Counted over 64 CPU packages on this system. "
                          "Parsing %s for core count may be inaccurate.",
                          file_name);
                cores = 0;
                break;
            }

            if (cpu & (1 << id)) {
                /* We've already counted this package's cores. */
                continue;
            }
            cpu |= 1 << id;

            /* Find the number of cores for this package. */
            while (fgets(line, sizeof line, stream)) {
                int count;

                if (pag_scan(line, "cpu cores%*[^:]: %u", &count)) {
                    cores += count;
                    break;
                }
            }
        }
    }
    fclose(stream);

    *n_cores = cores;
}

/* Returns the total number of cores on this system, or 0 if the number cannot
 * be determined.
 *
 * Tries not to count hyper-threads, but may be inaccurate - particularly on
 * platforms that do not provide /proc/cpuinfo, but also if /proc/cpuinfo is
 * formatted different to the layout that parse_cpuinfo() expects. */
int
count_cpu_cores(void)
{
    static struct pagthread_once once = PAGTHREAD_ONCE_INITIALIZER;
    static long int n_cores;

    if (pagthread_once_start(&once)) {
        parse_cpuinfo(&n_cores);
        if (!n_cores) {
            n_cores = sysconf(_SC_NPROCESSORS_ONLN);
        }
        pagthread_once_done(&once);
    }

    return n_cores > 0 ? n_cores : 0;
}

/* pagthread_key. */

#define L1_SIZE 1024
#define L2_SIZE 1024
#define MAX_KEYS (L1_SIZE * L2_SIZE)

/* A piece of thread-specific data. */
struct pagthread_key {
    struct list list_node;      /* In 'inuse_keys' or 'free_keys'. */
    void (*destructor)(void *); /* Called at thread exit. */

    /* Indexes into the per-thread array in struct pagthread_key_slots.
     * This key's data is stored in p1[index / L2_SIZE][index % L2_SIZE]. */
    unsigned int index;
};

/* Per-thread data structure. */
struct pagthread_key_slots {
    struct list list_node;      /* In 'slots_list'. */
    void **p1[L1_SIZE];
};

/* Contains "struct pagthread_key_slots *". */
static pthread_key_t tsd_key;

/* Guards data structures below. */
static struct pag_mutex key_mutex = PAG_MUTEX_INITIALIZER;

/* 'inuse_keys' holds "struct pagthread_key"s that have been created and not
 * yet destroyed.
 *
 * 'free_keys' holds "struct pagthread_key"s that have been deleted and are
 * ready for reuse.  (We keep them around only to be able to easily locate
 * free indexes.)
 *
 * Together, 'inuse_keys' and 'free_keys' hold an pagthread_key for every index
 * from 0 to n_keys - 1, inclusive. */
static struct list inuse_keys PAG_GUARDED_BY(key_mutex)
    = LIST_INITIALIZER(&inuse_keys);
static struct list free_keys PAG_GUARDED_BY(key_mutex)
    = LIST_INITIALIZER(&free_keys);
static unsigned int n_keys PAG_GUARDED_BY(key_mutex);

/* All existing struct pagthread_key_slots. */
static struct list slots_list PAG_GUARDED_BY(key_mutex)
    = LIST_INITIALIZER(&slots_list);

static void *
clear_slot(struct pagthread_key_slots *slots, unsigned int index)
{
    void **p2 = slots->p1[index / L2_SIZE];
    if (p2) {
        void **valuep = &p2[index % L2_SIZE];
        void *value = *valuep;
        *valuep = NULL;
        return value;
    } else {
        return NULL;
    }
}

static void
pagthread_key_destruct__(void *slots_)
{
    struct pagthread_key_slots *slots = slots_;
    struct pagthread_key *key;
    unsigned int n;
    int i;

    pag_mutex_lock(&key_mutex);
    list_remove(&slots->list_node);
    LIST_FOR_EACH (key, list_node, &inuse_keys) {
        void *value = clear_slot(slots, key->index);
        if (value && key->destructor) {
            key->destructor(value);
        }
    }
    n = n_keys;
    pag_mutex_unlock(&key_mutex);

    for (i = 0; i < n / L2_SIZE; i++) {
        free(slots->p1[i]);
    }
    free(slots);
}

/* Initializes '*keyp' as a thread-specific data key.  The data items are
 * initially null in all threads.
 *
 * If a thread exits with non-null data, then 'destructor', if nonnull, will be
 * called passing the final data value as its argument.  'destructor' must not
 * call any thread-specific data functions in this API.
 *
 * This function is similar to xpthread_key_create(). */
void
pagthread_key_create(pagthread_key_t *keyp, void (*destructor)(void *))
{
    static struct pagthread_once once = PAGTHREAD_ONCE_INITIALIZER;
    struct pagthread_key *key;

    if (pagthread_once_start(&once)) {
        xpthread_key_create(&tsd_key, pagthread_key_destruct__);
        pagthread_once_done(&once);
    }

    pag_mutex_lock(&key_mutex);
    if (list_is_empty(&free_keys)) {
        key = xmalloc(sizeof *key);
        key->index = n_keys++;
        if (key->index >= MAX_KEYS) {
            abort();
        }
    } else {
        key = CONTAINER_OF(list_pop_back(&free_keys),
                            struct pagthread_key, list_node);
    }
    list_push_back(&inuse_keys, &key->list_node);
    key->destructor = destructor;
    pag_mutex_unlock(&key_mutex);

    *keyp = key;
}

/* Frees 'key'.  The destructor supplied to pagthread_key_create(), if any, is
 * not called.
 *
 * This function is similar to xpthread_key_delete(). */
void
pagthread_key_delete(pagthread_key_t key)
{
    struct pagthread_key_slots *slots;

    pag_mutex_lock(&key_mutex);

    /* Move 'key' from 'inuse_keys' to 'free_keys'. */
    list_remove(&key->list_node);
    list_push_back(&free_keys, &key->list_node);

    /* Clear this slot in all threads. */
    LIST_FOR_EACH (slots, list_node, &slots_list) {
        clear_slot(slots, key->index);
    }

    pag_mutex_unlock(&key_mutex);
}

static void **
pagthread_key_lookup__(const struct pagthread_key *key)
{
    struct pagthread_key_slots *slots;
    void **p2;

    slots = pthread_getspecific(tsd_key);
    if (!slots) {
        slots = xzalloc(sizeof *slots);

        pag_mutex_lock(&key_mutex);
        pthread_setspecific(tsd_key, slots);
        list_push_back(&slots_list, &slots->list_node);
        pag_mutex_unlock(&key_mutex);
    }

    p2 = slots->p1[key->index / L2_SIZE];
    if (!p2) {
        p2 = xzalloc(L2_SIZE * sizeof *p2);
        slots->p1[key->index / L2_SIZE] = p2;
    }

    return &p2[key->index % L2_SIZE];
}

/* Sets the value of thread-specific data item 'key', in the current thread, to
 * 'value'.
 *
 * This function is similar to pthread_setspecific(). */
void
pagthread_setspecific(pagthread_key_t key, const void *value)
{
    *pagthread_key_lookup__(key) = CONST_CAST(void *, value);
}

/* Returns the value of thread-specific data item 'key' in the current thread.
 *
 * This function is similar to pthread_getspecific(). */
void *
pagthread_getspecific(pagthread_key_t key)
{
    return *pagthread_key_lookup__(key);
}
#endif

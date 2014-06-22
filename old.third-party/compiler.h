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
 * Copyright (c) 2008, 2009, 2010, 2011, 2012, 2013 Nicira, Inc.
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

#ifndef COMPILER_H
#define COMPILER_H 1

#define DO_PRAGMA(x) _Pragma (#x)
#define TODO(x) DO_PRAGMA(message ("TODO - " #x))
#define STRINGIFY(s) XSTRINGIFY(s)
#define XSTRINGIFY(s) #s

#ifndef __has_feature
  #define __has_feature(x) 0
#endif
#ifndef __has_extension
  #define __has_extension(x) 0
#endif

#if __GNUC__ && !__CHECKER__
#define NO_RETURN __attribute__((__noreturn__))
#define PAG_UNUSED __attribute__((__unused__))
#define PRINTF_FORMAT(FMT, ARG1) __attribute__((__format__(printf, FMT, ARG1)))
#define SCANF_FORMAT(FMT, ARG1) __attribute__((__format__(scanf, FMT, ARG1)))
#define STRFTIME_FORMAT(FMT) __attribute__((__format__(__strftime__, FMT, 0)))
#define MALLOC_LIKE __attribute__((__malloc__))
#define ALWAYS_INLINE __attribute__((always_inline))
#define WARN_UNUSED_RESULT __attribute__((__warn_unused_result__))
#define SENTINEL(N) __attribute__((sentinel(N)))
#define PAG_LIKELY(CONDITION) __builtin_expect(!!(CONDITION), 1)
#define PAG_UNLIKELY(CONDITION) __builtin_expect(!!(CONDITION), 0)
#else
#define NO_RETURN
#define PAG_UNUSED
#define PRINTF_FORMAT(FMT, ARG1)
#define SCANF_FORMAT(FMT, ARG1)
#define STRFTIME_FORMAT(FMT)
#define MALLOC_LIKE
#define ALWAYS_INLINE
#define WARN_UNUSED_RESULT
#define SENTINEL(N)
#define PAG_LIKELY(CONDITION) (!!(CONDITION))
#define PAG_UNLIKELY(CONDITION) (!!(CONDITION))
#endif

#if __has_feature(c_thread_safety_attributes)
/* "clang" annotations for thread safety check.
 *
 * PAG_LOCKABLE indicates that the struct contains mutex element
 * which can be locked by functions like pag_mutex_lock().
 *
 * Below, the word MUTEX stands for the name of an object with an PAG_LOCKABLE
 * struct type.  It can also be a comma-separated list of multiple structs,
 * e.g. to require a function to hold multiple locks while invoked.
 *
 *
 * On a variable:
 *
 *    - PAG_GUARDED indicates that the variable may only be accessed some mutex
 *      is held.
 *
 *    - PAG_GUARDED_BY(MUTEX) indicates that the variable may only be accessed
 *      while the specific MUTEX is held.
 *
 *
 * On a variable A of mutex type:
 *
 *    - PAG_ACQ_BEFORE(B), where B is a mutex or a comma-separated list of
 *      mutexes, declare that if both A and B are acquired at the same time,
 *      then A must be acquired before B.  That is, B nests inside A.
 *
 *    - PAG_ACQ_AFTER(B) is the opposite of PAG_ACQ_BEFORE(B), that is, it
 *      declares that A nests inside B.
 *
 *
 * On a function, the following attributes apply to mutexes:
 *
 *    - PAG_ACQUIRES(MUTEX) indicate that the function must be called without
 *      holding MUTEX and that it returns holding MUTEX.
 *
 *    - PAG_RELEASES(MUTEX) indicates that the function may only be called with
 *      MUTEX held and that it returns with MUTEX released.  It can be used for
 *      all types of MUTEX.
 *
 *    - PAG_TRY_LOCK(RETVAL, MUTEX) indicate that the function will try to
 *      acquire MUTEX.  RETVAL is an integer or boolean value specifying the
 *      return value of a successful lock acquisition.
 *
 *    - PAG_REQUIRES(MUTEX) indicate that the function may only be called with
 *      MUTEX held and that the function does not release MUTEX.
 *
 *    - PAG_EXCLUDED(MUTEX) indicates that the function may only be called when
 *      MUTEX is not held.
 *
 *
 * The following variants, with the same syntax, apply to reader-writer locks:
 *
 *    mutex                rwlock, for reading  rwlock, for writing
 *    -------------------  -------------------  -------------------
 *    PAG_ACQUIRES         PAG_ACQ_RDLOCK       PAG_ACQ_WRLOCK
 *    PAG_RELEASES         PAG_RELEASES         PAG_RELEASES
 *    PAG_TRY_LOCK         PAG_TRY_RDLOCK       PAG_TRY_WRLOCK
 *    PAG_REQUIRES         PAG_REQ_RDLOCK       PAG_REQ_WRLOCK
 *    PAG_EXCLUDED         PAG_EXCLUDED         PAG_EXCLUDED
 */
#define PAG_LOCKABLE __attribute__((lockable))
#define PAG_REQ_RDLOCK(...) __attribute__((shared_locks_required(__VA_ARGS__)))
#define PAG_ACQ_RDLOCK(...) __attribute__((shared_lock_function(__VA_ARGS__)))
#define PAG_REQ_WRLOCK(...) \
    __attribute__((exclusive_locks_required(__VA_ARGS__)))
#define PAG_ACQ_WRLOCK(...) \
    __attribute__((exclusive_lock_function(__VA_ARGS__)))
#define PAG_REQUIRES(...) \
    __attribute__((exclusive_locks_required(__VA_ARGS__)))
#define PAG_ACQUIRES(...) \
    __attribute__((exclusive_lock_function(__VA_ARGS__)))
#define PAG_TRY_WRLOCK(RETVAL, ...)                              \
    __attribute__((exclusive_trylock_function(RETVAL, __VA_ARGS__)))
#define PAG_TRY_RDLOCK(RETVAL, ...)                          \
    __attribute__((shared_trylock_function(RETVAL, __VA_ARGS__)))
#define PAG_TRY_LOCK(RETVAL, ...)                                \
    __attribute__((exclusive_trylock_function(RETVAL, __VA_ARGS__)))
#define PAG_GUARDED __attribute__((guarded_var))
#define PAG_GUARDED_BY(...) __attribute__((guarded_by(__VA_ARGS__)))
#define PAG_RELEASES(...) __attribute__((unlock_function(__VA_ARGS__)))
#define PAG_EXCLUDED(...) __attribute__((locks_excluded(__VA_ARGS__)))
#define PAG_ACQ_BEFORE(...) __attribute__((acquired_before(__VA_ARGS__)))
#define PAG_ACQ_AFTER(...) __attribute__((acquired_after(__VA_ARGS__)))
#define PAG_NO_THREAD_SAFETY_ANALYSIS \
    __attribute__((no_thread_safety_analysis))
#else  /* not Clang */
#define PAG_LOCKABLE
#define PAG_REQ_RDLOCK(...)
#define PAG_ACQ_RDLOCK(...)
#define PAG_REQ_WRLOCK(...)
#define PAG_ACQ_WRLOCK(...)
#define PAG_REQUIRES(...)
#define PAG_ACQUIRES(...)
#define PAG_TRY_WRLOCK(...)
#define PAG_TRY_RDLOCK(...)
#define PAG_TRY_LOCK(...)
#define PAG_GUARDED
#define PAG_GUARDED_BY(...)
#define PAG_EXCLUDED(...)
#define PAG_RELEASES(...)
#define PAG_ACQ_BEFORE(...)
#define PAG_ACQ_AFTER(...)
#define PAG_NO_THREAD_SAFETY_ANALYSIS
#endif

/* ISO C says that a C implementation may choose any integer type for an enum
 * that is sufficient to hold all of its values.  Common ABIs (such as the
 * System V ABI used on i386 GNU/Linux) always use a full-sized "int", even
 * when a smaller type would suffice.
 *
 * In GNU C, "enum __attribute__((packed)) name { ... }" defines 'name' as an
 * enum compatible with a type that is no bigger than necessary.  This is the
 * intended use of PAG_PACKED_ENUM.
 *
 * PAG_PACKED_ENUM is intended for use only as a space optimization, since it
 * only works with GCC.  That means that it must not be used in wire protocols
 * or otherwise exposed outside of a single process. */
#if __GNUC__ && !__CHECKER__
#define PAG_PACKED_ENUM __attribute__((__packed__))
#else
#define PAG_PACKED_ENUM
#endif

#ifndef _MSC_VER
#define PAG_PACKED(DECL) DECL __attribute__((__packed__))
#else
#define PAG_PACKED(DECL) __pragma(pack(push, 1)) DECL __pragma(pack(pop))
#endif

#ifdef _MSC_VER
#define CCALL __cdecl
#pragma section(".CRT$XCU",read)
#define PAG_CONSTRUCTOR(f) \
    static void __cdecl f(void); \
    __declspec(allocate(".CRT$XCU")) void (__cdecl*f##_)(void) = f; \
    static void __cdecl f(void)
#else
#define PAG_CONSTRUCTOR(f) \
    static void f(void) __attribute__((constructor)); \
    static void f(void)
#endif

#endif /* compiler.h */

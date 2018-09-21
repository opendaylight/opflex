#ifndef _MMAP_ALLOCATOR_H
#define _MMAP_ALLOCATOR_H

/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Alec Benzer
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 */

#include <memory>
#include <string>
#include <vector>
#include <map>
#include <cmath>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include <sys/uio.h>

#include <iostream>

#include <boost/atomic.hpp>

namespace yajr {
  namespace internal {
    namespace mm {

#define MAX_THREADS 4
static inline size_t GetThreadIndex() {
    static boost::atomic<size_t> threadCount(0);
    static __thread size_t myThreadIndex(0);

    /* this is ugly, but necessary because the NXOS toolchain has a compiler that
     * is even older than the compiler that is being used for the APIC and mininet...
     * if you try and do everything in one shot in the myThreadIndex declaration
     * line, the build will succeed in mininet and then fail when submitting to
     * sanity :(                                                 -Alessandro-
     */
    static __thread bool indexInitialized(false);
    if (!indexInitialized) {
        indexInitialized = true;
        myThreadIndex = threadCount++;
        if (myThreadIndex >= MAX_THREADS) {
            // crash immediately, even in final builds
            *(char*)0=0;
        }
    }
    return myThreadIndex;
}


const size_t kPageSize = 4096;

struct PageStats {
    uint16_t in_use;
    uint16_t recycled;
};

extern PageStats pages[MAX_THREADS];

extern void* default_allocator;

void SetDefault(std::string filename);
void SetDefault();
void FreeDefault();

void * alloc(size_t sz);
void free(void *p);

template <typename T>
inline bool random_failure(size_t n) { return false; }

template < > inline bool random_failure<iovec*>(size_t n) {
  //if ((rand()%3)) return false;
  //return true;
    return false;
}

template < > inline bool random_failure<iovec>(size_t n) { return false; }
template < > inline bool random_failure<char>(size_t n) { return false; }

template <typename T>
inline void failure(size_t n) {
    if (!random_failure<T>(n)) return;
    std::cout << __PRETTY_FUNCTION__ << " " << n << " times " << sizeof(T) << " = " << n * sizeof(T) << "\n";
    throw std::bad_alloc();
}

class FreeMap {
  public:
    std::map<size_t, std::vector<void*> > free_[MAX_THREADS];
};

template <typename T>
class Allocator {
 public:
  typedef T value_type;
  typedef T* pointer;
  typedef const T* const_pointer;
  typedef T& reference;
  typedef const T& const_reference;
  typedef std::size_t size_type;
  typedef off_t difference_type;

//typedef std::map<void*, size_type> SizeMap;

  template <class U>
  friend class Allocator;

  template <class U>
  struct rebind {
    typedef Allocator<U> other;
  };

  static Allocator* New(std::string filename) {
    int fd = open(filename.c_str(), O_RDWR | O_CREAT | O_TRUNC, (mode_t)0777);
    if (fd == -1) {
      return NULL;
    }

    return new Allocator(fd, /* new SizeMap(), */ new FreeMap());
  }

  static Allocator* New() {
    return new Allocator(-1, /* new SizeMap(), */ new FreeMap());
  }

  Allocator()
      : fd_(static_cast<Allocator*>(default_allocator)->fd_),
     /* sizes_(static_cast<Allocator*>(default_allocator)->sizes_), */
        free_blocks_(static_cast<Allocator*>(default_allocator)->free_blocks_) {
  }

  template <class U>
  Allocator(const Allocator<U>& other)
      : fd_(other.fd_),
     /* sizes_(other.sizes_), */
        free_blocks_(other.free_blocks_) {}

  static inline size_t byteSize(size_t n) {
    size_t to_alloc = n * sizeof(T);

    if (to_alloc + 256 < kPageSize) {
        // favor malloc for small allocations
        return to_alloc;
    }

    // round up to multiple of page size
    if (to_alloc % kPageSize != 0) {
      to_alloc = ((to_alloc / kPageSize) + 1) * kPageSize;
    }

    return to_alloc;
  }

  T* allocate(size_t n) {
    size_t to_alloc = byteSize(n);

    /* attempt small allocations via malloc() */
    if (to_alloc < kPageSize) {
        T* addr = static_cast<T*>(malloc(to_alloc));
#if __cpp_exceptions || __EXCEPTIONS
failure<T>(n);
        if (addr)
#endif
            return addr;
        /* it would be great to fall through, but if malloc() is failing
         * then the map insertion at the end would most likely be failing
         * too */
#if __cpp_exceptions || __EXCEPTIONS
        throw std::bad_alloc();
#endif
    }

    auto& vec = (*free_blocks_).free_[GetThreadIndex()][to_alloc];
    if (!vec.empty()) {
      T* addr = static_cast<T*>(vec.back());
      vec.pop_back();
      ++pages[GetThreadIndex()].in_use;
      --pages[GetThreadIndex()].recycled;
      return addr;
    }

    /* some implementations may want -1 as an offset for MAP_ANONYMOUS */
    off_t previous_end = 0;
    if (-1 != fd_) {
        previous_end = lseek(fd_, 0, SEEK_END);

        if (lseek(fd_, to_alloc - 1, SEEK_END) == -1) {
          return NULL;
        }

        if (write(fd_, "", 1) != 1) {
          return NULL;
        }
    }
#if __cpp_exceptions || __EXCEPTIONS
failure<T>(n);
#endif
    void * retval = mmap(NULL, to_alloc, PROT_READ | PROT_WRITE, ((-1==fd_)?(MAP_PRIVATE | MAP_ANONYMOUS):MAP_SHARED), fd_, previous_end);

    if (retval == MAP_FAILED) {
#if __cpp_exceptions || __EXCEPTIONS
      throw std::bad_alloc();
#else
      return NULL;
#endif
    }

    T* addr = static_cast<T*>(retval);
 // (*sizes_)[addr] = to_alloc;
    ++pages[GetThreadIndex()].in_use;
    return addr;
  }

  void deallocate(T* p, size_t n) {
    auto to_free = byteSize(n); // (*sizes_)[(void*)p];

    if (to_free < kPageSize) {
        ::free(static_cast<void*>(p));
        return;
    }

    (*free_blocks_).free_[GetThreadIndex()][to_free].push_back((void*)p);
    ++pages[GetThreadIndex()].recycled;
    --pages[GetThreadIndex()].in_use;
  }

  void construct(pointer p, const_reference val) { new ((void*)p) T(val); }

  void destroy(pointer p) { p->~T(); }

  template <class U, class... Args>
  void construct(U* p, Args&&... args) {
    ::new ((void*)p) U(std::forward<Args>(args)...);
  }

  template <class U>
  void destroy(U* p) {
    p->~U();
  }

  int const &  fd() const {
    return fd_;
  }

  Allocator(int fd, /* SizeMap* sizes, */ FreeMap* free_blocks)
      : fd_(fd), /* sizes_(sizes), */ free_blocks_(free_blocks) {
          if (kPageSize != sysconf(_SC_PAGE_SIZE)){
              // crash immediately, even in final builds
              *(char*)0=0;
          }
      }

 private:
  int fd_;
  // a map from allocated addresses to the "actual" sizes of their allocations
  // (as opposed to just what was requested, as requests are rounded up the
  // nearest page size multiple)
//std::shared_ptr<SizeMap> sizes_;
  std::shared_ptr<FreeMap> free_blocks_;
};

template <typename T, typename U>
inline bool operator==(const Allocator<T>& a, const Allocator<U>& b) {
  return a.fd() == b.fd();
}

template <typename T, typename U>
inline bool operator!=(const Allocator<T>& a, const Allocator<U>& b) {
  return !(a == b);
}

template <size_t N>
class PreAllocator
{
  public:
    PreAllocator()
    {
        char * chunk[N];

        Allocator<char>* all = static_cast<Allocator<char>*>(default_allocator);

        for(size_t i = 0; i < N; ++i){
            chunk[i] = all->allocate(4096);
            *chunk[i] = '0'; // write into the first byte to make sure the kernel finds a backing physical page
        }
        for(size_t i = 0; i < N; ++i){
            all->deallocate(chunk[i], 4096);
        }
    }
};


};  // namespace mm
}}

#endif

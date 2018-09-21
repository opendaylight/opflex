#pragma once
#ifndef _COMMS__INCLUDE__YAJR__INTERNAL__BUFFERS_HPP
#define _COMMS__INCLUDE__YAJR__INTERNAL__BUFFERS_HPP

#include <sys/uio.h>

#include <deque>
#include <vector>

#include <yajr/internal/mmap_allocator.hpp>

#include <boost/static_assert.hpp>

#include <cstdlib>
#include <ctime>


namespace yajr {
    namespace internal {

const size_t kBufSizeRT = sysconf(_SC_PAGE_SIZE);
const size_t kBufSize = 4096;   // faster to have an immediate value
const uintptr_t kMask = ~0xfff; // faster to have an immediate value


template < class T, class Alloc = mm::Allocator<T> >
class Buffers {
  public:
    Buffers(const Alloc &a = Alloc())
        :
            alloc_(a),
            pending_(0)
#if !(__cpp_exceptions || __EXCEPTIONS)
          , flushed_(false)
#endif
    {
        // crash immediately, even in final builds, if page size detected
        // at runtime doesn't match the one compiled in!
        if (kBufSize != kBufSizeRT) *(char*)0=0;

#if 0
#ifndef _GLIBCXX_DEQUE_BUF_SIZE

# error "_GLIBCXX_DEQUE_BUF_SIZE is not defined. Should be defined to be 4096 or else we'll get the default 512 which is bad"

#endif
      BOOST_STATIC_ASSERT_MSG(
          (kBufSize == _GLIBCXX_DEQUE_BUF_SIZE),
          "stdlibc++ deque buffer size doesn't match expected page size"
      );
#endif
        BOOST_STATIC_ASSERT_MSG(
            (kBufSize == (~kMask)+1),
            "buffer size and buffer address mask are mismatching"
        );

    }

    ~Buffers() {
        clear();
    }

    size_t size() {
        size_t iov_size = iov_.size();

        switch(iov_size) {
            case 0:
                return 0;
            case 1:
                return iov_.back().iov_len;
            default:
                iov_size -= 2;
                iov_size *= kBufSize;
                iov_size += iov_.front().iov_len;
                iov_size += iov_.back().iov_len;
                return iov_size;
        }
    }

    size_t pending() {
        return pending_;
    }

    void consumed(ssize_t howManyS = -1) {
        bool disposed;

        size_t howMany = (howManyS < 0) ? pending_ : howManyS;

        while(howMany) {
            iovec & iov = iov_.front();
            if (iov.iov_len >  howMany) {
                iov.iov_len -= howMany;
                /* the ?: optimization below will need to be removed if in the future we allow to concurrently produce
                 * and consume data */
                iov.iov_base = iov.iov_len ? reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(iov.iov_base) + howMany)
                                           : reinterpret_cast<void *>(reinterpret_cast<uintptr_t>(iov.iov_base) & kMask)
                                           ;
                howMany = 0;
                break;
            }
            howMany -= iov.iov_len;
         // disposed = dispose_if_full(iov);
         // assert(disposed);
            dispose(iov);
            iov_.pop_front();
        }
        assert(!howMany);
        pending_ = 0;
    }

    void clear() {
        pending_ = size();
        consumed();
    }

    std::vector<iovec> get_iovec() {
        static std::vector<iovec> const empty_iov;

        assert(!pending_);
        if (pending_) {
            return empty_iov;
        }
        pending_ = size();

        {
            /* check that the last byte was a frame terminator */
            iovec & back = iov_.back();
            assert(!*(reinterpret_cast< char * >(back.iov_base) + back.iov_len - 1));
        }

        std::vector<iovec> v(iov_.cbegin(), iov_.cend());
        return v;
    }

    static uintptr_t is_full(iovec const & iov) {
        uintptr_t const p = reinterpret_cast<uintptr_t>(iov.iov_base) + iov.iov_len;
        if (iov.iov_len && !(p % kBufSize)) {
            return p - kBufSize;
        }
        return 0;
    }

    size_t flush_memory() {
        shrink_to_fit();
#if !(__cpp_exceptions || __EXCEPTIONS)
        flushed_ = true;
#endif

        size_t flushed = size() - pending_;
        size_t skip = pending_;
        ssize_t skbuf;

        if (!skip) {

            // delete all iovec's
            skbuf = -1;

        } else {

            // we need a special case for the first iovec, since it may not be 0-based

            iovec & fb = iov_.front();
            if (skip < fb.iov_len) {    // maybe also <= would be good

                fb.iov_len = skip;
                // delete all subsequent iovec's
                skbuf = 0;

            } else {

                skip -= fb.iov_len;

                skbuf               = 1 + skip / kBufSize;
                iov_[skbuf].iov_len =     skip % kBufSize;
            }
        }

        for(ssize_t i = iov_.size() - 1; i > skbuf; --i) {
            iovec & iov = iov_.back();
            dispose(iov);
            iov_.pop_back();
        }

        return flushed;
    }

    void append(T c) {

#if !(__cpp_exceptions || __EXCEPTIONS)
        if (__builtin_expect((flushed_), false)) {
            if (c == '\0') {
                flushed_ = false;
            } else {
                return;
            }
        }
#endif

        if (!iov_.size() || is_full(iov_.back())) {

#if __cplusplus < 201103L
            iovec const iov = {
                alloc_.allocate(kBufSize),
                0
            };
            if (!iov.iov_base) {
                out_of_memory();

#if !(__cpp_exceptions || __EXCEPTIONS)
              return;
#endif
            }

            iov_.push_back(iov);
#else
            void * p = alloc_.allocate(kBufSize);
            if (!p) {
              out_of_memory();

#if !(__cpp_exceptions || __EXCEPTIONS)
              return;
#endif
            }
            iov_.emplace_back();
            iov_.back().iov_base = p;
#endif
        }
        iovec & iov = iov_.back();
        assert(iov.iov_len <= 4096);
        assert(iov.iov_len >= 0);
        static_cast<T *>(iov.iov_base)[iov.iov_len++] = c;
    }

    void shrink_to_fit() {
        /* This is not very useful */
#if __cplusplus >= 201103L
        iov_.shrink_to_fit();
#endif
    }

private:
    void dispose(iovec const & iov) {
        alloc_.deallocate(reinterpret_cast<char *>(reinterpret_cast<uintptr_t>(iov.iov_base) & kMask), kBufSize);
    }

    bool dispose_if_full(iovec const & iov) const {
        uintptr_t const p = is_full(iov);
        if (p) {
            alloc_.deallocate(reinterpret_cast<T *>(p), kBufSize);
        }
        return static_cast<bool>(p);
    }

    inline void out_of_memory() const {
#if __cpp_exceptions || __EXCEPTIONS
        throw std::bad_alloc();
#else
        (void) const_cast< Buffers< T > * >(this)->flush_memory();
#endif
    }

    /* TODO: use double-buffer vector instead? */
    std::deque<iovec, mm::Allocator<iovec> > iov_;
    Alloc alloc_;
    size_t pending_;
#if !(__cpp_exceptions || __EXCEPTIONS)
    bool flushed_;
#endif
};

}
}

#endif /* _COMMS__INCLUDE__YAJR__INTERNAL__BUFFERS_HPP */

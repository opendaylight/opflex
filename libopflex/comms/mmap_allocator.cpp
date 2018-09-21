#include <yajr/internal/mmap_allocator.hpp>
#include <sys/types.h>
#include <unistd.h>
#include <cstring>
#include <deque>


namespace yajr {
  namespace internal {
    namespace mm {

FreeMap freeMap;

PageStats pages[MAX_THREADS];
void* default_allocator = static_cast<void*>(new Allocator<char>(-1, &freeMap));

#ifdef ALLOC_PEER_MMAP_FILE
#ifndef ALLOC_MMAP_FILE_PREFIX
#define ALLOC_MMAP_FILE_PREFIX "/tmp/opflexp-alloc"
#endif
char filename_cutter[] = ALLOC_MMAP_FILE_PREFIX "/00000/tmp_XXXXXXXXXXXXXX";
pid_t opflex_pid = getpid();
int filename_len = (
		   snprintf(filename_cutter, sizeof(filename_cutter), ALLOC_MMAP_FILE_PREFIX),
                   mkdir(filename_cutter, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH),
		   snprintf(filename_cutter, sizeof(filename_cutter), ALLOC_MMAP_FILE_PREFIX "/%05d", opflex_pid),
                   mkdir(filename_cutter, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH),
		   1 + snprintf(filename_cutter, sizeof(filename_cutter), ALLOC_MMAP_FILE_PREFIX "/%05d/tmp_XXXXXXXXXXXXXX", opflex_pid)
		   );
#endif

void SetDefault(std::string filename)
{
    if (!default_allocator)
    {
        default_allocator = static_cast<void*>(Allocator<char>::New(filename));
    }
}

void SetDefault()
{
    if (!default_allocator)
    {
        default_allocator = static_cast<void*>(Allocator<char>::New());
    }
}

void FreeDefault()
{
    if (default_allocator)
    {
        delete static_cast<Allocator<char>*>(default_allocator);
        default_allocator = NULL;
    }
}

void * alloc(size_t sz)
{
    int fd = -1;

#ifdef ALLOC_PEER_MMAP_FILE
    char filename[filename_len];
    (void) memcpy(filename, filename_cutter, sizeof(filename));

    sz = (sz + 4095) &~4095;

    fd = mkstemp(filename);


    if (-1 != fd) {
        if (lseek(fd, sz - 1, SEEK_END) == -1) {
          return NULL;
        }

        if (write(fd, "", 1) != 1) {
          return NULL;
        }
    }
#endif

    /* add one 4096 bytes page at the end, without backing store, so as to trigger SIGBUS upon buffer overflow */
    void * retval = mmap(NULL, sz + 4096, PROT_READ | PROT_WRITE, ((-1==fd)?(MAP_PRIVATE | MAP_ANONYMOUS):MAP_SHARED), fd, 0);
#ifdef ALLOC_PEER_MMAP_FILE
    if (-1 != fd) {
        (void) close(fd);

        char dst[filename_len];
        (void) snprintf(dst, sizeof(dst), ALLOC_MMAP_FILE_PREFIX "/%05d/%p", opflex_pid, retval);

        int re = rename(filename, dst);
    }
#endif

    if (MAP_FAILED == retval) {
        return NULL;
    }

    return retval;
}

void free(void *p)
{
    /* TODO: consider locking in case one day Peers get freed from multiple threads */
    static std::deque<void *> freed;

    size_t const len = 4096;  // in C++14 we will be able to get the len as a parameter
    (void) mprotect(p, (len + 4095) & ~4095, PROT_NONE);
    (void) msync(p, (len + 4095) & ~4095, MS_ASYNC);

    while (freed.size() > 999) {
        void * garbage = freed.front();
        freed.pop_front();
        munmap(garbage, 4096);

#ifdef ALLOC_PEER_MMAP_FILE
        char fname[filename_len];
        (void) snprintf(fname, sizeof(fname), "/tmp/opflexp-alloc/%05d/%p", opflex_pid, garbage);

        remove(fname);
#endif
    }

    freed.push_back(p);
}

}
}}

// syscall.hpp — 直接 svc 调用，绕开 libc 的 PLT 探针与 syscall() 包装
#pragma once
#include <cstdint>
#include <sys/types.h>

namespace sc {

#if defined(__aarch64__)
__attribute__((always_inline))
static inline long raw(long n, long a1 = 0, long a2 = 0, long a3 = 0,
                       long a4 = 0, long a5 = 0, long a6 = 0) {
    register long x8 __asm__("x8") = n;
    register long x0 __asm__("x0") = a1;
    register long x1 __asm__("x1") = a2;
    register long x2 __asm__("x2") = a3;
    register long x3 __asm__("x3") = a4;
    register long x4 __asm__("x4") = a5;
    register long x5 __asm__("x5") = a6;
    __asm__ volatile("svc #0"
                     : "+r"(x0)
                     : "r"(x8), "r"(x1), "r"(x2), "r"(x3), "r"(x4), "r"(x5)
                     : "memory");
    return x0;
}
#else
extern "C" long syscall(long, ...);
__attribute__((always_inline))
static inline long raw(long n, long a1 = 0, long a2 = 0, long a3 = 0,
                       long a4 = 0, long a5 = 0, long a6 = 0) {
    return ::syscall(n, a1, a2, a3, a4, a5, a6);
}
#endif

// aarch64 syscall numbers
enum : long {
    SYS_openat     = 56,
    SYS_close      = 57,
    SYS_read       = 63,
    SYS_write      = 64,
    SYS_readlinkat = 78,
    SYS_faccessat  = 48,
    SYS_newfstatat = 79,
    SYS_getpid     = 172,
    SYS_gettid     = 178,
    SYS_tgkill     = 131,
    SYS_dup        = 23,
    SYS_dup3       = 24,
    SYS_mmap       = 222,
    SYS_munmap     = 215,
    SYS_mprotect   = 226,
    SYS_memfd_create = 279,
    SYS_prctl      = 167,
    SYS_ioctl      = 29,
    SYS_lseek      = 62,
    SYS_ftruncate  = 46,
    SYS_nanosleep  = 101,
};

inline int   sys_openat(int dfd, const char* p, int fl, int mode) { return (int)raw(SYS_openat, dfd, (long)p, fl, mode); }
inline int   sys_close(int fd)                                    { return (int)raw(SYS_close, fd); }
inline long  sys_read(int fd, void* b, size_t n)                  { return raw(SYS_read, fd, (long)b, n); }
inline long  sys_write(int fd, const void* b, size_t n)           { return raw(SYS_write, fd, (long)b, n); }
inline int   sys_mprotect(void* a, size_t l, int p)               { return (int)raw(SYS_mprotect, (long)a, l, p); }
inline int   sys_memfd(const char* name, unsigned flags)          { return (int)raw(SYS_memfd_create, (long)name, flags); }
inline long  sys_lseek(int fd, long off, int whence)              { return raw(SYS_lseek, fd, off, whence); }

} // namespace sc

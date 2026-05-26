#ifndef LIBC_INTERNAL_H
#define LIBC_INTERNAL_H
#include <errno.h>
#include <lamp/libsys.h>

extern int errno;
static inline void set_errno_from_libsys(void) { errno = libsys_errno; }
static inline int ret_errno(int rc) {
    if (rc < 0) { set_errno_from_libsys(); return -1; }
    errno = 0; return rc;
}
#endif

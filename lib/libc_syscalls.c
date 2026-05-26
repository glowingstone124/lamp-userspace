#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/statfs.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>

#include <lamp/libsys.h>
#include "libc_internal.h"

/* ---- internal helpers ---- */
int errno;
static char *g_environ_empty[] = { 0 };
char **environ = g_environ_empty;

/* ---- process ---- */
pid_t getpid(void) { return (pid_t)ret_errno(libsys_getpid()); }
pid_t getppid(void) { return (pid_t)ret_errno(libsys_getppid()); }
uid_t getuid(void) { return 0; }
uid_t geteuid(void) { return 0; }
gid_t getgid(void) { return 0; }
gid_t getegid(void) { return 0; }
int getpagesize(void) { return 4096; }
pid_t setsid(void) { return getpid(); }
pid_t getsid(pid_t pid) { (void)pid; return getpid(); }
pid_t getpgrp(void) { return getpid(); }
int setpgid(pid_t pid, pid_t pgid) { (void)pid; (void)pgid; return 0; }
int setpgrp(void) { return 0; }

/* ---- fork/exec ---- */
pid_t vfork(void) { return (pid_t)ret_errno(libsys_vfork()); }
int execve(const char *path, char *const argv[], char *const envp[]) {
    return ret_errno(libsys_execve(path, (const char *const *)argv, (const char *const *)envp));
}
int execv(const char *path, char *const argv[]) { return execve(path, argv, environ); }
int execvp(const char *file, char *const argv[]) {
    const char *path_env; char full_path[256];
    if (!file) { errno = EFAULT; return -1; }
    if (strchr(file, '/')) return execve(file, argv, environ);
    path_env = getenv("PATH"); if (!path_env) path_env = "/bin";
    while (*path_env) {
        const char *end = strchr(path_env, ':');
        size_t dir_len = end ? (size_t)(end - path_env) : strlen(path_env);
        if (dir_len == 0) { path_env++; continue; }
        if (dir_len + 1 + strlen(file) + 1 > sizeof(full_path)) { path_env = end ? end + 1 : path_env + dir_len; continue; }
        memcpy(full_path, path_env, dir_len); full_path[dir_len] = '/';
        strcpy(full_path + dir_len + 1, file);
        execve(full_path, argv, environ);
        if (errno != ENOENT) break;
        path_env = end ? end + 1 : path_env + dir_len;
    }
    return -1;
}

/* ---- wait ---- */
pid_t waitpid(pid_t pid, int *status, int options) {
    int rc = libsys_waitpid(pid, status, (uint32_t)(options & WNOHANG));
    if (rc < 0) { set_errno_from_libsys(); if (pid == (pid_t)-1 && (options & WNOHANG)) errno = ECHILD; return (pid_t)-1; }
    errno = 0; return (pid_t)rc;
}
pid_t wait(int *status) { return waitpid(-1, status, 0); }

/* ---- I/O ---- */
int open(const char *path, int flags, ...) { return ret_errno(libsys_open(path, (uint32_t)flags)); }
int close(int fd) { return ret_errno(libsys_close(fd)); }
ssize_t read(int fd, void *buf, size_t count) { return (ssize_t)ret_errno(libsys_read(fd, buf, (uint32_t)count)); }
ssize_t write(int fd, const void *buf, size_t count) { return (ssize_t)ret_errno(libsys_write(fd, buf, (uint32_t)count)); }
off_t lseek(int fd, off_t offset, int whence) { return (off_t)ret_errno(libsys_lseek(fd, offset, (uint32_t)whence)); }
int dup(int oldfd) { return ret_errno(libsys_dup(oldfd)); }
int dup2(int oldfd, int newfd) { return ret_errno(libsys_dup2(oldfd, newfd)); }
int pipe(int pipefd[2]) { return ret_errno(libsys_pipe(pipefd)); }
int isatty(int fd) { return fd >= 0 && fd <= 2; }
int ttyname_r(int fd, char *buf, size_t size) { (void)fd; if (buf && size) buf[0] = '\0'; errno = ENOTTY; return -1; }
int tcsetpgrp(int fd, pid_t pgrp) { (void)fd; (void)pgrp; return 0; }
pid_t tcgetpgrp(int fd) { (void)fd; return getpid(); }

int fcntl(int fd, int cmd, ...) {
    va_list ap; uint32_t arg = 0; int rc;
    va_start(ap, cmd); arg = va_arg(ap, uint32_t); va_end(ap);
    rc = libsys_fcntl(fd, (uint32_t)cmd, arg);
    if (rc < 0) { set_errno_from_libsys(); if ((cmd == F_DUPFD || cmd == F_DUPFD_CLOEXEC) && arg >= 32u) errno = EBADF; return -1; }
    errno = 0; return rc;
}

int ioctl(int fd, unsigned long request, ...) {
    va_list ap; void *arg;
    va_start(ap, request); arg = va_arg(ap, void *); va_end(ap);
    return ret_errno(libsys_ioctl(fd, (uint32_t)request, arg));
}

/* ---- file system ---- */
int chdir(const char *path) { return ret_errno(libsys_chdir(path)); }
char *getcwd(char *buf, size_t size) {
    int rc = libsys_getcwd(buf, (uint32_t)size);
    if (rc < 0) { set_errno_from_libsys(); return 0; }
    errno = 0; return buf;
}
int access(const char *path, int mode) { return ret_errno(libsys_access(path, (uint32_t)mode)); }
int unlink(const char *path) { return ret_errno(libsys_unlink(path)); }
int rmdir(const char *path) { return ret_errno(libsys_rmdir(path)); }
int link(const char *oldpath, const char *newpath) { return ret_errno(libsys_link(oldpath, newpath)); }
int symlink(const char *target, const char *linkpath) { return ret_errno(libsys_symlink(target, linkpath)); }
ssize_t readlink(const char *path, char *buf, size_t size) { return (ssize_t)ret_errno(libsys_readlink(path, buf, (uint32_t)size)); }
int mkdir(const char *path, mode_t mode) { return ret_errno(libsys_mkdir(path, mode)); }
int rename(const char *oldpath, const char *newpath) { return ret_errno(libsys_rename(oldpath, newpath)); }
mode_t umask(mode_t mask) { return (mode_t)libsys_umask(mask); }

/* ---- stat ---- */
static void stat_copy(struct stat *dst, const lamp_stat_t *src) {
    dst->st_dev = src->st_dev; dst->st_ino = src->st_ino; dst->st_mode = src->st_mode;
    dst->st_nlink = src->st_nlink; dst->st_uid = src->st_uid; dst->st_gid = src->st_gid;
    dst->st_rdev = src->st_rdev; dst->st_size = (off_t)src->st_size;
    dst->st_blksize = src->st_blksize; dst->st_blocks = src->st_blocks;
    dst->st_atime = 0; dst->st_mtime = 0; dst->st_ctime = 0;
}
int stat(const char *path, struct stat *st) {
    lamp_stat_t lst; int rc = libsys_stat(path, &lst);
    if (rc < 0) { set_errno_from_libsys(); return -1; }
    stat_copy(st, &lst); errno = 0; return 0;
}
int lstat(const char *path, struct stat *st) { return stat(path, st); }
int fstat(int fd, struct stat *st) {
    lamp_stat_t lst; int rc = libsys_fstat(fd, &lst);
    if (rc < 0) { set_errno_from_libsys(); return -1; }
    stat_copy(st, &lst); errno = 0; return 0;
}

/* ---- poll/select ---- */
int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    return ret_errno(libsys_call6(LAMP_SYS_POLL, (uint32_t)(uintptr_t)fds, (uint32_t)nfds, (uint32_t)timeout, 0, 0, 0));
}
int select(int nfds, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout) {
    int timeout_ms = -1;
    if (timeout) timeout_ms = (int)(timeout->tv_sec * 1000u + (uint32_t)timeout->tv_usec / 1000u);
    return ret_errno(libsys_call6(LAMP_SYS_SELECT, (uint32_t)nfds, (uint32_t)(uintptr_t)readfds,
                                  (uint32_t)(uintptr_t)writefds, (uint32_t)(uintptr_t)exceptfds, (uint32_t)timeout_ms, 0));
}

/* ---- sysconf ---- */
long sysconf(int name) { if (name == _SC_CLK_TCK) return 100; return -1; }

/* ---- misc stubs ---- */
int getgroups(int size, gid_t list[]) { if (size > 0 && list) list[0] = 0; return 1; }
int chown(const char *path, uid_t owner, gid_t group) { (void)path; (void)owner; (void)group; return 0; }
int lchown(const char *path, uid_t owner, gid_t group) { return chown(path, owner, group); }
int chmod(const char *path, mode_t mode) { (void)path; (void)mode; return 0; }
int mknod(const char *path, mode_t mode, dev_t dev) { (void)path; (void)mode; (void)dev; errno = ENOSYS; return -1; }
int utimes(const char *path, const struct timeval times[2]) { (void)path; (void)times; return 0; }
int chroot(const char *path) { (void)path; errno = ENOSYS; return -1; }
int fchdir(int fd) { (void)fd; return 0; }
int mkstemp(char *template) { (void)template; errno = ENOSYS; return -1; }
int ftruncate(int fd, off_t length) { (void)fd; (void)length; return 0; }
char *realpath(const char *path, char *resolved_path) {
    if (!path || !resolved_path) { errno = EINVAL; return 0; }
    strcpy(resolved_path, path); return resolved_path;
}
int setuid(uid_t uid) { (void)uid; return 0; }
int setgid(gid_t gid) { (void)gid; return 0; }
int seteuid(uid_t uid) { (void)uid; return 0; }
int setegid(gid_t gid) { (void)gid; return 0; }
int setresuid(uid_t ruid, uid_t euid, uid_t suid) { (void)ruid; (void)euid; (void)suid; return 0; }
int setresgid(gid_t rgid, gid_t egid, gid_t sgid) { (void)rgid; (void)egid; (void)sgid; return 0; }
int getrlimit(int resource, struct rlimit *rlim) { (void)resource; if (rlim) { rlim->rlim_cur = 32; rlim->rlim_max = 32; } return 0; }
int setrlimit(int resource, const struct rlimit *rlim) { (void)resource; (void)rlim; return 0; }
void *mmap(void *addr, unsigned long len, int prot, int flags, int fd, long off) { (void)addr; (void)prot; (void)flags; (void)fd; (void)off; return malloc(len); }
int munmap(void *addr, unsigned long len) { (void)len; free(addr); return 0; }
int sched_getaffinity(pid_t pid, size_t cpusetsize, void *mask) { (void)pid; if (cpusetsize >= sizeof(unsigned long) && mask) { memset(mask, 0, cpusetsize); *(unsigned long *)mask = 1; return 0; } errno = EINVAL; return -1; }
int statfs(const char *path, struct statfs *buf) { (void)path; if (buf) memset(buf, 0, sizeof(*buf)); return 0; }
int sysinfo(struct sysinfo *info) { if (info) memset(info, 0, sizeof(*info)); return 0; }
int uname(struct utsname *buf) {
    if (!buf) { errno = EFAULT; return -1; }
    strcpy(buf->sysname, "Octans"); strcpy(buf->nodename, "lamp-VirtualMachine");
    strcpy(buf->release, "Alpha"); strcpy(buf->version, "0.31"); strcpy(buf->machine, "Polaris");
    return 0;
}

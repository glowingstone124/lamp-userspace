#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <fnmatch.h>
#include <getopt.h>
#include <glob.h>
#include <grp.h>
#include <netdb.h>
#include <net/if.h>
#include <netinet/in.h>
#include <pwd.h>
#include <regex.h>
#include <setjmp.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <syslog.h>
#include <termios.h>
#include <unistd.h>

#include <lamp/libsys.h>
#include "libc_internal.h"

/* ---- network stubs ---- */
int h_errno;

int inet_aton(const char *cp, struct in_addr *inp) {
    unsigned int parts[4] = {0, 0, 0, 0}; const char *p = cp; char *end; int i;
    if (!cp || !inp) return 0;
    for (i = 0; i < 4; i++) {
        unsigned long v; if (*p == '\0') return 0;
        v = strtoul(p, &end, 10); if (end == p || v > 255) return 0;
        parts[i] = (unsigned int)v;
        if (i == 3) { if (*end != '\0') return 0; break; }
        if (*end != '.') return 0; p = end + 1;
    }
    inp->s_addr = htonl((parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3]);
    return 1;
}

char *inet_ntoa(struct in_addr in) { static char buf[16]; unsigned int a = ntohl(in.s_addr); snprintf(buf, sizeof(buf), "%u.%u.%u.%u", (a >> 24) & 0xff, (a >> 16) & 0xff, (a >> 8) & 0xff, a & 0xff); return buf; }
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    if (!src || !dst || size == 0) {
        errno = EINVAL;
        return 0;
    }
    if (af == AF_INET) {
        unsigned int a = ntohl(((const struct in_addr *)src)->s_addr);
        int n = snprintf(dst, (size_t)size, "%u.%u.%u.%u",
                         (a >> 24) & 0xff, (a >> 16) & 0xff,
                         (a >> 8) & 0xff, a & 0xff);
        if (n < 0 || (unsigned int)n >= (unsigned int)size) {
            errno = ENOSPC;
            return 0;
        }
        return dst;
    }
    errno = EAFNOSUPPORT;
    return 0;
}
int inet_pton(int af, const char *src, void *dst) {
    if (!src || !dst) {
        errno = EINVAL;
        return -1;
    }
    if (af == AF_INET) {
        return inet_aton(src, (struct in_addr *)dst);
    }
    errno = EAFNOSUPPORT;
    return -1;
}

int setsockopt(int fd, int level, int optname, const void *optval, socklen_t optlen) { (void)fd; (void)level; (void)optname; (void)optval; (void)optlen; return 0; }
int getsockopt(int fd, int level, int optname, void *optval, socklen_t *optlen) { (void)fd; (void)level; (void)optname; (void)optval; (void)optlen; return -1; }
int getpeername(int fd, struct sockaddr *addr, socklen_t *len) { (void)fd; (void)addr; (void)len; return -1; }
int getsockname(int fd, struct sockaddr *addr, socklen_t *len) { (void)fd; (void)addr; (void)len; errno = ENOSYS; return -1; }

struct servent *getservbyname(const char *name, const char *proto) { (void)name; (void)proto; return 0; }
struct servent *getservbyport(int port, const char *proto) { (void)port; (void)proto; return 0; }
int getaddrinfo(const char *node, const char *service, const struct addrinfo *hints, struct addrinfo **res) { (void)node; (void)service; (void)hints; (void)res; return EAI_FAIL; }
void freeaddrinfo(struct addrinfo *res) { (void)res; }
int getnameinfo(const struct sockaddr *addr, socklen_t addrlen, char *host, socklen_t hostlen, char *serv, socklen_t servlen, int flags) { (void)addr; (void)addrlen; (void)host; (void)hostlen; (void)serv; (void)servlen; (void)flags; return EAI_FAIL; }

struct hostent *gethostbyname(const char *name) {
    static struct in_addr addr; static char *aliases[] = { 0 };
    static char *addr_list[] = { (char *)&addr, 0 };
    static struct hostent host = { 0, aliases, AF_INET, sizeof(addr), addr_list };
    if (!name) { h_errno = HOST_NOT_FOUND; return 0; }
    if (strcmp(name, "localhost") == 0) addr.s_addr = htonl(INADDR_LOOPBACK);
    else if (!inet_aton(name, &addr)) { h_errno = HOST_NOT_FOUND; return 0; }
    host.h_name = (char *)name; h_errno = NETDB_SUCCESS;
    return &host;
}
const char *hstrerror(int err) {
    switch (err) {
        case NETDB_SUCCESS: return "Resolver Error 0 (no error)";
        case HOST_NOT_FOUND: return "Unknown host"; case TRY_AGAIN: return "Host name lookup failure";
        case NO_RECOVERY: return "Unknown server error"; case NO_DATA: return "No address associated with name";
        default: return "Resolver error";
    }
}

int socket(int domain, int type, int protocol) { return ret_errno(libsys_call6(LAMP_SYS_SOCKET, (uint32_t)domain, (uint32_t)type, (uint32_t)protocol, 0, 0, 0)); }
int connect(int fd, const struct sockaddr *addr, socklen_t len) { return ret_errno(libsys_call6(LAMP_SYS_CONNECT, (uint32_t)fd, (uint32_t)(uintptr_t)addr, len, 0, 0, 0)); }
int bind(int fd, const struct sockaddr *addr, socklen_t len) { return ret_errno(libsys_call6(LAMP_SYS_BIND, (uint32_t)fd, (uint32_t)(uintptr_t)addr, len, 0, 0, 0)); }
int listen(int fd, int backlog) { return ret_errno(libsys_call6(LAMP_SYS_LISTEN, (uint32_t)fd, (uint32_t)backlog, 0, 0, 0, 0)); }
int accept(int fd, struct sockaddr *addr, socklen_t *len) { return ret_errno(libsys_call6(LAMP_SYS_ACCEPT, (uint32_t)fd, (uint32_t)(uintptr_t)addr, (uint32_t)(uintptr_t)len, 0, 0, 0)); }
int shutdown(int fd, int how) { (void)fd; (void)how; return 0; }
ssize_t send(int fd, const void *buf, size_t len, int flags) { return (ssize_t)ret_errno(libsys_call6(LAMP_SYS_SEND, (uint32_t)fd, (uint32_t)(uintptr_t)buf, (uint32_t)len, (uint32_t)flags, 0, 0)); }
ssize_t recv(int fd, void *buf, size_t len, int flags) { return (ssize_t)ret_errno(libsys_call6(LAMP_SYS_RECV, (uint32_t)fd, (uint32_t)(uintptr_t)buf, (uint32_t)len, (uint32_t)flags, 0, 0)); }
ssize_t sendto(int fd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen) { return (ssize_t)ret_errno(libsys_call6(LAMP_SYS_SEND, (uint32_t)fd, (uint32_t)(uintptr_t)buf, (uint32_t)len, (uint32_t)flags, (uint32_t)(uintptr_t)dest_addr, (uint32_t)addrlen)); }
ssize_t recvfrom(int fd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen) { (void)src_addr; (void)addrlen; return recv(fd, buf, len, flags); }

unsigned short htons(unsigned short x) { return (unsigned short)((x << 8) | (x >> 8)); }
unsigned short ntohs(unsigned short x) { return htons(x); }
unsigned int htonl(unsigned int x) { return __builtin_bswap32(x); }
unsigned int ntohl(unsigned int x) { return htonl(x); }
unsigned int if_nametoindex(const char *name) { (void)name; return 0; }
char *if_indextoname(unsigned int ifindex, char *name) { (void)ifindex; if (name) name[0] = '\0'; return name; }

/* ---- dirent ---- */
typedef struct { int fd; struct dirent ent; } lamp_DIR;
struct DIR { lamp_DIR d; };

DIR *opendir(const char *path) {
    int fd = open(path, O_RDONLY); if (fd < 0) return 0;
    DIR *d = malloc(sizeof(DIR)); if (!d) { close(fd); return 0; }
    d->d.fd = fd; return d;
}
struct dirent *readdir(DIR *dirp) {
    lamp_dirent_t lent; int rc;
    if (!dirp) return 0;
    rc = libsys_getdents(dirp->d.fd, &lent, sizeof(lent));
    if (rc <= 0) return 0;
    dirp->d.ent.d_ino = lent.d_ino; dirp->d.ent.d_off = (long)lent.d_off;
    dirp->d.ent.d_reclen = (unsigned short)lent.d_reclen; dirp->d.ent.d_type = (unsigned char)lent.d_type;
    strncpy(dirp->d.ent.d_name, lent.d_name, sizeof(dirp->d.ent.d_name));
    return &dirp->d.ent;
}
int closedir(DIR *dirp) { int rc = dirp ? close(dirp->d.fd) : -1; free(dirp); return rc; }

/* ---- passwd/group stubs ---- */
struct passwd *getpwuid(uid_t uid) { (void)uid; return 0; }
struct passwd *getpwnam(const char *name) { (void)name; return 0; }
struct passwd *bb_internal_getpwuid(uid_t uid) { return getpwuid(uid); }
struct passwd *bb_internal_getpwnam(const char *name) { return getpwnam(name); }
struct passwd *bb_internal_getpwent(void) { return 0; }
void bb_internal_setpwent(void) {}
void bb_internal_endpwent(void) {}
void endpwent(void) {}
struct group *getgrgid(gid_t gid) { (void)gid; return 0; }
struct group *getgrnam(const char *name) { (void)name; return 0; }
struct group *bb_internal_getgrgid(gid_t gid) { return getgrgid(gid); }
struct group *bb_internal_getgrnam(const char *name) { return getgrnam(name); }
int initgroups(const char *user, gid_t group) { (void)user; (void)group; return 0; }
void endgrent(void) {}

/* ---- fnmatch/glob/regex stubs ---- */
int fnmatch(const char *pattern, const char *string, int flags) { (void)flags; return strcmp(pattern, string) == 0 ? 0 : FNM_NOMATCH; }
int glob(const char *pattern, int flags, int (*errfunc)(const char *, int), glob_t *pglob) { (void)pattern; (void)flags; (void)errfunc; if (pglob) { pglob->gl_pathc = 0; pglob->gl_pathv = 0; pglob->gl_offs = 0; } return GLOB_NOMATCH; }
void globfree(glob_t *pglob) { (void)pglob; }

/* ---- getopt ---- */
char *optarg; int optind = 1; int opterr = 1; int optopt;
static char *g_getopt_pos;

int getopt(int argc, char *const argv[], const char *optstring) {
    int c;
    if (optind == 0) { optind = 1; g_getopt_pos = 0; }
    optarg = 0;
    if (!g_getopt_pos || *g_getopt_pos == '\0') {
        if (optind >= argc || !argv[optind] || argv[optind][0] != '-' || argv[optind][1] == '\0') return -1;
        if (argv[optind][1] == '-' && argv[optind][2] == '\0') { optind++; return -1; }
        g_getopt_pos = argv[optind] + 1;
    }
    c = (unsigned char)*g_getopt_pos++;
    const char *spec = strchr(optstring, c); optopt = c;
    if (!spec || c == ':') { if (*g_getopt_pos == '\0') optind++; return '?'; }
    if (spec[1] == ':') {
        if (*g_getopt_pos != '\0') { optarg = g_getopt_pos; optind++; g_getopt_pos = 0; }
        else if (optind + 1 < argc) { optarg = argv[++optind]; optind++; g_getopt_pos = 0; }
        else { optind++; return optstring[0] == ':' ? ':' : '?'; }
    } else if (*g_getopt_pos == '\0') { optind++; g_getopt_pos = 0; }
    return c;
}
int getopt_long(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex) {
    (void)longopts; if (longindex) *longindex = -1; return getopt(argc, argv, optstring);
}
int getopt_long_only(int argc, char *const argv[], const char *optstring, const struct option *longopts, int *longindex) {
    return getopt_long(argc, argv, optstring, longopts, longindex);
}

/* ---- termios stubs ---- */
int tcgetattr(int fd, struct termios *t) { (void)fd; (void)t; errno = ENOTTY; return -1; }
int tcsetattr(int fd, int optional_actions, const struct termios *t) { (void)fd; (void)optional_actions; (void)t; errno = ENOTTY; return -1; }
int tcflush(int fd, int queue_selector) { (void)fd; (void)queue_selector; return 0; }

/* ---- syslog stubs ---- */
void openlog(const char *ident, int option, int facility) { (void)ident; (void)option; (void)facility; }
void vsyslog(int priority, const char *format, va_list ap) { (void)priority; vfprintf(stderr, format, ap); fputc('\n', stderr); }
void syslog(int priority, const char *format, ...) { va_list ap; va_start(ap, format); vsyslog(priority, format, ap); va_end(ap); }
void closelog(void) {}

/* ---- misc ---- */
char *dirname(char *path) { char *slash = strrchr(path, '/'); if (!slash) return "."; if (slash == path) { path[1] = '\0'; return path; } *slash = '\0'; return path; }
char *basename(char *path) { char *slash = strrchr(path, '/'); return slash ? slash + 1 : path; }

/* ---- setjmp/longjmp ---- */
__asm__(
    ".text\n.globl setjmp\nsetjmp:\n"
    "  store32 r30, r0, 0\n  store32 r31, r0, 4\n"
    "  movi r1, 0xF0\n  in r2, r1\n  store32 r2, r0, 8\n"
    "  movi r1, 0xF4\n  in r3, r1\n  add r4, r2, r2\n"
    "  add r4, r4, r4\n  add r4, r4, r4\n  add r3, r3, r4\n"
    "  load32 r4, r3, 0\n  store32 r4, r0, 12\n  movi r0, 0\n  ret\n"
    ".globl longjmp\nlongjmp:\n"
    "  load32 r30, r0, 0\n  load32 r31, r0, 4\n"
    "  load32 r2, r0, 8\n  load32 r3, r0, 12\n"
    "  movi r4, 0xF4\n  in r5, r4\n  add r6, r2, r2\n"
    "  add r6, r6, r6\n  add r6, r6, r6\n  add r5, r5, r6\n"
    "  store32 r3, r5, 0\n  movi r6, 0\n  store32 r6, r5, 4\n"
    "  mov r0, r1\n  movi r4, 0xF0\n  out r2, r4\n  ret\n"
);

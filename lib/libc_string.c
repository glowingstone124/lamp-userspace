#include <ctype.h>
#include <errno.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

void *memcpy(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    for (size_t i = 0; i < n; i++) d[i] = s[i];
    return dst;
}

void *memmove(void *dst, const void *src, size_t n) {
    unsigned char *d = dst;
    const unsigned char *s = src;
    if (d < s) { for (size_t i = 0; i < n; i++) d[i] = s[i]; }
    else { while (n) { n--; d[n] = s[n]; } }
    return dst;
}

void *memset(void *dst, int c, size_t n) {
    unsigned char *d = dst;
    for (size_t i = 0; i < n; i++) d[i] = (unsigned char)c;
    return dst;
}

int memcmp(const void *a, const void *b, size_t n) {
    const unsigned char *x = a, *y = b;
    for (size_t i = 0; i < n; i++) if (x[i] != y[i]) return (int)x[i] - (int)y[i];
    return 0;
}

void *memchr(const void *s, int c, size_t n) {
    const unsigned char *p = s;
    for (size_t i = 0; i < n; i++) if (p[i] == (unsigned char)c) return (void *)(p + i);
    return 0;
}

void *mempcpy(void *dst, const void *src, size_t n) { return (char *)memcpy(dst, src, n) + n; }

size_t strlen(const char *s) { size_t n = 0; while (s && s[n]) n++; return n; }
size_t strnlen(const char *s, size_t maxlen) { size_t n = 0; while (s && n < maxlen && s[n]) n++; return n; }

char *strcpy(char *dst, const char *src) { char *d = dst; while ((*d++ = *src++) != 0) {} return dst; }
char *strncpy(char *dst, const char *src, size_t n) {
    size_t i = 0;
    for (; i < n && src[i]; i++) dst[i] = src[i];
    for (; i < n; i++) dst[i] = 0;
    return dst;
}
char *strcat(char *dst, const char *src) { strcpy(dst + strlen(dst), src); return dst; }
char *stpcpy(char *dst, const char *src) { while ((*dst = *src) != 0) { dst++; src++; } return dst; }

int strcmp(const char *a, const char *b) { while (*a && *a == *b) { a++; b++; } return (unsigned char)*a - (unsigned char)*b; }
int strncmp(const char *a, const char *b, size_t n) {
    for (size_t i = 0; i < n; i++) { if (a[i] != b[i] || a[i] == 0 || b[i] == 0) return (unsigned char)a[i] - (unsigned char)b[i]; }
    return 0;
}
int strcasecmp(const char *a, const char *b) { while (*a && *b) { int d = tolower(*a) - tolower(*b); if (d) return d; a++; b++; } return tolower(*a) - tolower(*b); }
int strncasecmp(const char *a, const char *b, size_t n) { size_t i = 0; while (i < n && *a && *b) { int d = tolower(*a) - tolower(*b); if (d) return d; a++; b++; i++; } return i == n ? 0 : tolower(*a) - tolower(*b); }
char *strcasestr(const char *h, const char *n) { while (*h) { size_t i = 0; while (n[i] && tolower((unsigned char)h[i]) == tolower((unsigned char)n[i])) i++; if (!n[i]) return (char *)h; h++; } return 0; }
int strcoll(const char *a, const char *b) { return strcmp(a, b); }
int strverscmp(const char *a, const char *b) { return strcmp(a, b); }

char *strchr(const char *s, int c) { while (*s) { if (*s == (char)c) return (char *)s; s++; } return c == 0 ? (char *)s : 0; }
char *strchrnul(const char *s, int c) { char *p = strchr(s, c); return p ? p : (char *)(s + strlen(s)); }
char *strrchr(const char *s, int c) {
    const char *last = 0; char ch = (char)c;
    for (;;) { if (*s == ch) last = s; if (*s == '\0') break; s++; }
    return (char *)last;
}
char *strstr(const char *h, const char *n) {
    size_t nl = strlen(n); if (nl == 0) return (char *)h;
    while (*h) { if (strncmp(h, n, nl) == 0) return (char *)h; h++; }
    return 0;
}
char *strtok_r(char *str, const char *delim, char **saveptr) {
    char *s = str ? str : (saveptr ? *saveptr : 0);
    char *end;
    if (!s || !delim || !saveptr) return 0;
    s += strspn(s, delim);
    if (*s == '\0') {
        *saveptr = s;
        return 0;
    }
    end = s + strcspn(s, delim);
    if (*end) {
        *end++ = '\0';
    }
    *saveptr = end;
    return s;
}
size_t strspn(const char *s, const char *accept) { size_t n = 0; while (s[n] && strchr(accept, s[n])) n++; return n; }
size_t strcspn(const char *s, const char *reject) { size_t n = 0; while (s[n] && !strchr(reject, s[n])) n++; return n; }
char *strdup(const char *s) { size_t n = strlen(s) + 1; char *p = malloc(n); if (p) memcpy(p, s, n); return p; }
char *strndup(const char *s, size_t n) { size_t len = strnlen(s, n); char *p = malloc(len + 1); if (p) { memcpy(p, s, len); p[len] = '\0'; } return p; }
char *strerror(int errnum) {
    switch (errnum) {
        case ENOENT: return "No such file"; case EINVAL: return "Invalid argument";
        case EBADF: return "Bad file descriptor"; case ENOSYS: return "Function not implemented";
        default: return "Error";
    }
}

int isdigit(int c) { return c >= '0' && c <= '9'; }
int isalpha(int c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }
int isalnum(int c) { return isalpha(c) || isdigit(c); }
int isxdigit(int c) { return isdigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F'); }
int isspace(int c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'; }
int isprint(int c) { return c >= 0x20 && c < 0x7f; }
int isupper(int c) { return c >= 'A' && c <= 'Z'; }
int islower(int c) { return c >= 'a' && c <= 'z'; }
int tolower(int c) { return isupper(c) ? c + ('a' - 'A') : c; }
int toupper(int c) { return islower(c) ? c - ('a' - 'A') : c; }

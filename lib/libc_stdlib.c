#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lamp/libsys.h>

/* heap: 256KB static arena */
static unsigned char g_heap[256 * 1024];
static size_t g_heap_used;

static size_t align4(size_t n) { return (n + 3u) & ~3u; }

void *malloc(size_t size) {
    size = align4(size ? size : 1u);
    size_t total = align4(size + sizeof(size_t));
    if (g_heap_used + total > sizeof(g_heap)) { errno = ENOMEM; return 0; }
    unsigned char *raw = &g_heap[g_heap_used];
    *(size_t *)raw = size;
    g_heap_used += total;
    return raw + sizeof(size_t);
}

void free(void *ptr) { (void)ptr; }

void *calloc(size_t nmemb, size_t size) {
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size) {
    void *p = malloc(size);
    if (p && ptr) {
        size_t old_size = *(size_t *)((unsigned char *)ptr - sizeof(size_t));
        size_t copy = old_size < size ? old_size : size;
        memcpy(p, ptr, copy);
    }
    return p;
}

long strtol(const char *nptr, char **endptr, int base) {
    int neg = 0; unsigned long v;
    while (isspace((unsigned char)*nptr)) nptr++;
    if (*nptr == '-') { neg = 1; nptr++; } else if (*nptr == '+') nptr++;
    v = strtoul(nptr, endptr, base);
    return neg ? -(long)v : (long)v;
}

unsigned long strtoul(const char *nptr, char **endptr, int base) {
    unsigned long v = 0;
    if (base == 0) base = 10;
    while (isspace((unsigned char)*nptr)) nptr++;
    while (*nptr) {
        int d;
        if (*nptr >= '0' && *nptr <= '9') d = *nptr - '0';
        else if (*nptr >= 'a' && *nptr <= 'z') d = *nptr - 'a' + 10;
        else if (*nptr >= 'A' && *nptr <= 'Z') d = *nptr - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * (unsigned)base + (unsigned)d;
        nptr++;
    }
    if (endptr) *endptr = (char *)nptr;
    return v;
}

long long strtoll(const char *nptr, char **endptr, int base) { return (long long)strtol(nptr, endptr, base); }
unsigned long long strtoull(const char *nptr, char **endptr, int base) { return (unsigned long long)strtoul(nptr, endptr, base); }
double strtod(const char *nptr, char **endptr) {
    const char *p = nptr;
    while (p && isspace((unsigned char)*p)) p++;
    if (p && (*p == '-' || *p == '+')) p++;
    while (p && isdigit((unsigned char)*p)) p++;
    if (p && *p == '.') { p++; while (isdigit((unsigned char)*p)) p++; }
    if (endptr) *endptr = (char *)(p ? p : nptr);
    return 0.0;
}

int atoi(const char *s) { return (int)strtol(s, 0, 10); }

static unsigned long g_rand_seed = 1;
int rand(void) { g_rand_seed = g_rand_seed * 1103515245u + 12345u; return (int)((g_rand_seed / 65536u) % (unsigned)(RAND_MAX + 1)); }
void srand(unsigned int seed) { g_rand_seed = seed; }

char *getenv(const char *name) { (void)name; return 0; }
int setenv(const char *name, const char *value, int overwrite) { (void)name; (void)value; (void)overwrite; return 0; }
int putenv(char *string) { (void)string; return 0; }
int unsetenv(const char *name) { (void)name; return 0; }
int clearenv(void) { return 0; }

void qsort(void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    unsigned char *b = base; unsigned char tmp[64];
    if (size > sizeof(tmp)) return;
    for (size_t i = 0; i < nmemb; i++)
        for (size_t j = i + 1; j < nmemb; j++)
            if (compar(&b[i * size], &b[j * size]) > 0) {
                memcpy(tmp, &b[i * size], size);
                memcpy(&b[i * size], &b[j * size], size);
                memcpy(&b[j * size], tmp, size);
            }
}

void *bsearch(const void *key, const void *base, size_t nmemb, size_t size, int (*compar)(const void *, const void *)) {
    size_t lo = 0, hi = nmemb;
    while (lo < hi) {
        size_t mid = lo + (hi - lo) / 2;
        int cmp = compar(key, (const char *)base + mid * size);
        if (cmp < 0) hi = mid;
        else if (cmp > 0) lo = mid + 1;
        else return (void *)((const char *)base + mid * size);
    }
    return 0;
}

void _exit(int status) { (void)libsys_exit(status); for (;;) {} }
void exit(int status) { _exit(status); }
void abort(void) { _exit(127); }

char *setlocale(int category, const char *locale) { (void)category; (void)locale; return "C"; }

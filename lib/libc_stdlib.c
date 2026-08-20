#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <lamp/libsys.h>

/* Small first-fit heap for the current fixed user address space. */
static unsigned char g_heap[256 * 1024];
typedef struct heap_block {
    size_t size;
    struct heap_block *next;
    unsigned char free;
} heap_block_t;
static heap_block_t *g_heap_head;
static unsigned char *g_heap_end = g_heap;

static size_t align4(size_t n) { return (n + 3u) & ~3u; }

void *malloc(size_t size) {
    heap_block_t *block;
    heap_block_t *tail = 0;
    size_t wanted = align4(size ? size : 1u);
    for (block = g_heap_head; block; block = block->next) {
        if (block->free && block->size >= wanted) {
            block->free = 0;
            return (unsigned char *)block + sizeof(*block);
        }
        tail = block;
    }
    if ((size_t)(g_heap + sizeof(g_heap) - g_heap_end) <
        align4(sizeof(heap_block_t) + wanted)) {
        errno = ENOMEM;
        return 0;
    }
    block = (heap_block_t *)g_heap_end;
    block->size = wanted;
    block->next = 0;
    block->free = 0;
    if (tail) tail->next = block;
    else g_heap_head = block;
    g_heap_end += align4(sizeof(*block) + wanted);
    return (unsigned char *)block + sizeof(*block);
}

void free(void *ptr) {
    heap_block_t *block;
    heap_block_t *previous = 0;
    if (!ptr || (unsigned char *)ptr < g_heap + sizeof(heap_block_t) ||
        (unsigned char *)ptr >= g_heap_end) return;
    block = (heap_block_t *)((unsigned char *)ptr - sizeof(*block));
    block->free = 1;
    for (block = g_heap_head; block; block = block->next) {
        if (previous && previous->free && block->free) {
            previous->size += align4(sizeof(*block) + block->size);
            previous->next = block->next;
            block = previous;
        }
        previous = block;
    }
}

void *calloc(size_t nmemb, size_t size) {
    if (size != 0 && nmemb > (size_t)-1 / size) { errno = ENOMEM; return 0; }
    size_t total = nmemb * size;
    void *p = malloc(total);
    if (p) memset(p, 0, total);
    return p;
}

void *realloc(void *ptr, size_t size) {
    heap_block_t *block;
    void *p;
    if (!ptr) return malloc(size);
    if (size == 0) { free(ptr); return 0; }
    block = (heap_block_t *)((unsigned char *)ptr - sizeof(*block));
    if (block->size >= size) return ptr;
    p = malloc(size);
    if (p && ptr) {
        size_t copy = block->size < size ? block->size : size;
        memcpy(p, ptr, copy);
        free(ptr);
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
    const char *start = nptr;
    unsigned long v = 0;
    int digits = 0;
    if (!nptr) { errno = EINVAL; if (endptr) *endptr = 0; return 0; }
    while (isspace((unsigned char)*nptr)) nptr++;
    if (base == 0) {
        base = 10;
        if (nptr[0] == '0') {
            base = 8;
            if (nptr[1] == 'x' || nptr[1] == 'X') { base = 16; nptr += 2; }
        }
    } else if (base == 16 && nptr[0] == '0' && (nptr[1] == 'x' || nptr[1] == 'X')) {
        nptr += 2;
    }
    if (base < 2 || base > 36) { errno = EINVAL; if (endptr) *endptr = (char *)start; return 0; }
    while (*nptr) {
        int d;
        if (*nptr >= '0' && *nptr <= '9') d = *nptr - '0';
        else if (*nptr >= 'a' && *nptr <= 'z') d = *nptr - 'a' + 10;
        else if (*nptr >= 'A' && *nptr <= 'Z') d = *nptr - 'A' + 10;
        else break;
        if (d >= base) break;
        v = v * (unsigned)base + (unsigned)d;
        nptr++;
        digits++;
    }
    if (!digits) {
        errno = EINVAL;
    }
    if (endptr) *endptr = (char *)(digits ? nptr : start);
    return v;
}

long long strtoll(const char *nptr, char **endptr, int base) { return (long long)strtol(nptr, endptr, base); }
unsigned long long strtoull(const char *nptr, char **endptr, int base) { return (unsigned long long)strtoul(nptr, endptr, base); }
double strtod(const char *nptr, char **endptr) {
    const char *p = nptr;
    if (!p) { errno = EINVAL; if (endptr) *endptr = 0; return 0.0; }
    while (isspace((unsigned char)*p)) p++;
    if (*p == '-' || *p == '+') p++;
    while (isdigit((unsigned char)*p)) p++;
    if (*p == '.') {
        p++;
        while (isdigit((unsigned char)*p)) p++;
    }
    if (*p == 'e' || *p == 'E') {
        const char *exp_start = p++;
        if (*p == '-' || *p == '+') p++;
        while (isdigit((unsigned char)*p)) p++;
        if (p == exp_start + 1 || (p == exp_start + 2 &&
                                   (exp_start[1] == '+' || exp_start[1] == '-'))) p = exp_start;
    }
    if (endptr) *endptr = (char *)p;
    return 0.0;
}

int atoi(const char *s) { return (int)strtol(s, 0, 10); }

static unsigned long g_rand_seed = 1;
int rand(void) { g_rand_seed = g_rand_seed * 1103515245u + 12345u; return (int)((g_rand_seed / 65536u) % (unsigned)(RAND_MAX + 1)); }
void srand(unsigned int seed) { g_rand_seed = seed; }

static int env_name_matches(const char *entry, const char *name, size_t name_len) {
    return entry && strncmp(entry, name, name_len) == 0 && entry[name_len] == '=';
}
static int env_find(const char *name, size_t name_len) {
    if (!name) return -1;
    for (int i = 0; environ && environ[i]; i++) {
        if (env_name_matches(environ[i], name, name_len)) return i;
    }
    return -1;
}
static int env_valid_name(const char *name, size_t *name_len) {
    size_t len;
    if (!name || !*name) { errno = EINVAL; return 0; }
    len = strlen(name);
    if (strchr(name, '=')) { errno = EINVAL; return 0; }
    if (name_len) *name_len = len;
    return 1;
}
char *getenv(const char *name) {
    size_t name_len;
    int index;
    if (!env_valid_name(name, &name_len)) return 0;
    index = env_find(name, name_len);
    return index < 0 ? 0 : strchr(environ[index], '=') + 1;
}
int setenv(const char *name, const char *value, int overwrite) {
    size_t name_len;
    int index;
    char *entry;
    if (!env_valid_name(name, &name_len) || !value) {
        if (!value) errno = EINVAL;
        return -1;
    }
    index = env_find(name, name_len);
    if (index >= 0 && !overwrite) return 0;
    entry = malloc(name_len + strlen(value) + 2);
    if (!entry) { errno = ENOMEM; return -1; }
    memcpy(entry, name, name_len);
    entry[name_len] = '=';
    strcpy(entry + name_len + 1, value);
    if (index >= 0) {
        environ[index] = entry;
        return 0;
    }
    {
        int count = 0;
        char **next;
        while (environ && environ[count]) count++;
        next = realloc(environ, (size_t)(count + 2) * sizeof(*next));
        if (!next) { errno = ENOMEM; return -1; }
        next[count] = entry;
        next[count + 1] = 0;
        environ = next;
    }
    return 0;
}
int putenv(char *string) {
    char *equals;
    char name[128];
    size_t name_len;
    if (!string || !(equals = strchr(string, '='))) { errno = EINVAL; return -1; }
    name_len = (size_t)(equals - string);
    if (name_len == 0 || name_len >= sizeof(name)) { errno = EINVAL; return -1; }
    memcpy(name, string, name_len);
    name[name_len] = '\0';
    return setenv(name, equals + 1, 1);
}
int unsetenv(const char *name) {
    size_t name_len;
    int index;
    int count;
    if (!env_valid_name(name, &name_len)) return -1;
    index = env_find(name, name_len);
    if (index < 0) return 0;
    count = 0;
    while (environ[count]) count++;
    for (; index < count; index++) environ[index] = environ[index + 1];
    return 0;
}
int clearenv(void) {
    static char *empty[] = { 0 };
    environ = empty;
    return 0;
}

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

static void (*g_exit_handlers[32])(void);
static unsigned g_exit_handler_count;
int atexit(void (*function)(void)) {
    if (!function || g_exit_handler_count >= sizeof(g_exit_handlers) / sizeof(g_exit_handlers[0])) {
        errno = ENOMEM;
        return -1;
    }
    g_exit_handlers[g_exit_handler_count++] = function;
    return 0;
}
void _exit(int status) { (void)libsys_exit(status); for (;;) {} }
void exit(int status) {
    while (g_exit_handler_count) g_exit_handlers[--g_exit_handler_count]();
    _exit(status);
}
void abort(void) { _exit(127); }

char *setlocale(int category, const char *locale) { (void)category; (void)locale; return "C"; }

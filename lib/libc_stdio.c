#include <errno.h>
#include <fcntl.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>

#include <lamp/libsys.h>

struct FILE { int fd; int eof; int err; };

static FILE g_stdin  = { 0, 0, 0 };
static FILE g_stdout = { 1, 0, 0 };
static FILE g_stderr = { 2, 0, 0 };
FILE *stdin  = &g_stdin;
FILE *stdout = &g_stdout;
FILE *stderr = &g_stderr;

static int file_fd(FILE *stream) { return stream ? stream->fd : 1; }

int fflush(FILE *stream) { (void)stream; return 0; }
int fileno(FILE *stream) { return file_fd(stream); }
int fileno_unlocked(FILE *stream) { return fileno(stream); }

FILE *fdopen(int fd, const char *mode) {
    (void)mode;
    FILE *f = malloc(sizeof(FILE));
    if (f) { f->fd = fd; f->eof = 0; f->err = 0; }
    return f;
}

FILE *fopen(const char *path, const char *mode) {
    int flags = O_RDONLY;
    if (mode && mode[0] == 'w') flags = O_WRONLY | O_CREAT | O_TRUNC;
    int fd = open(path, flags, 0666);
    return fd < 0 ? 0 : fdopen(fd, mode);
}

FILE *freopen(const char *path, const char *mode, FILE *stream) {
    int flags = O_RDONLY; int fd;
    if (!stream) return fopen(path, mode);
    if (mode && mode[0] == 'w') flags = O_WRONLY | O_CREAT | O_TRUNC;
    fd = open(path, flags, 0666);
    if (fd < 0) return 0;
    close(stream->fd); stream->fd = fd; stream->eof = 0; stream->err = 0;
    return stream;
}

int fclose(FILE *stream) {
    int fd = file_fd(stream);
    if (stream != stdin && stream != stdout && stream != stderr) free(stream);
    return close(fd);
}

int fseeko(FILE *stream, off_t offset, int whence) { return lseek(file_fd(stream), offset, whence) < 0 ? -1 : 0; }

size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    ssize_t n = read(file_fd(stream), ptr, size * nmemb);
    if (n <= 0) { if (stream) stream->eof = 1; return 0; }
    return (size_t)n / size;
}

size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream) {
    ssize_t n = write(file_fd(stream), ptr, size * nmemb);
    return n <= 0 ? 0 : (size_t)n / size;
}

char *fgets(char *s, int size, FILE *stream) {
    if (size <= 0) return 0;
    int i = 0;
    while (i + 1 < size) { char c; if (read(file_fd(stream), &c, 1) != 1) break; s[i++] = c; if (c == '\n') break; }
    if (i == 0) return 0;
    s[i] = '\0';
    return s;
}

ssize_t getline(char **lineptr, size_t *n, FILE *stream) {
    size_t cap; size_t len = 0; int c; char *buf;
    if (!lineptr || !n) { errno = EINVAL; return -1; }
    cap = *n ? *n : 128;
    buf = *lineptr ? *lineptr : malloc(cap);
    if (!buf) { errno = ENOMEM; return -1; }
    while ((c = fgetc(stream)) != EOF) {
        if (len + 1 >= cap) { cap *= 2; char *nb = realloc(buf, cap); if (!nb) { errno = ENOMEM; return -1; } buf = nb; }
        buf[len++] = (char)c; if (c == '\n') break;
    }
    if (len == 0 && c == EOF) { if (!*lineptr) free(buf); return -1; }
    buf[len] = '\0'; *lineptr = buf; *n = cap;
    return (ssize_t)len;
}

int feof(FILE *stream) { return stream ? stream->eof : 1; }
int feof_unlocked(FILE *stream) { return feof(stream); }
int ferror(FILE *stream) { return stream ? stream->err : 1; }
int ferror_unlocked(FILE *stream) { return ferror(stream); }
void clearerr(FILE *stream) { if (stream) { stream->eof = 0; stream->err = 0; } }

int fputs(const char *s, FILE *stream) { return write(file_fd(stream), s, strlen(s)) < 0 ? EOF : 0; }
int fputs_unlocked(const char *s, FILE *stream) { return fputs(s, stream); }
int puts(const char *s) { if (fputs(s, stdout) == EOF) return EOF; return fputs("\n", stdout); }

int fputc(int c, FILE *stream) { unsigned char ch = (unsigned char)c; return write(file_fd(stream), &ch, 1) == 1 ? c : EOF; }
int putc(int c, FILE *stream) { return fputc(c, stream); }
int putc_unlocked(int c, FILE *stream) { return fputc(c, stream); }
int putchar(int c) { return fputc(c, stdout); }
int putchar_unlocked(int c) { return fputc(c, stdout); }

int fgetc(FILE *stream) { unsigned char ch; return read(file_fd(stream), &ch, 1) == 1 ? ch : EOF; }
int getc(FILE *stream) { return fgetc(stream); }
int getc_unlocked(FILE *stream) { return fgetc(stream); }
int getchar(void) { return fgetc(stdin); }
int getchar_unlocked(void) { return getchar(); }
char *fgets_unlocked(char *s, int size, FILE *stream) { return fgets(s, size, stream); }

void perror(const char *s) {
    if (s) { fputs(s, stderr); fputs(": ", stderr); }
    fputs(strerror(errno), stderr); fputs("\n", stderr);
}

/* ---- printf family ---- */
static void out_char(char **buf, size_t *left, int fd, int *count, char c) {
    if (buf) { if (*left > 1) { **buf = c; (*buf)++; (*left)--; } }
    else { (void)write(fd, &c, 1); }
    (*count)++;
}
static void out_mem(char **buf, size_t *left, int fd, int *count, const char *s, size_t len) {
    while (len--) out_char(buf, left, fd, count, *s++);
}
static void out_repeat(char **buf, size_t *left, int fd, int *count, char c, int n) {
    while (n-- > 0) out_char(buf, left, fd, count, c);
}
static void out_padded(char **buf, size_t *left, int fd, int *count,
                       const char *s, int len, int width, int left_adjust, char pad) {
    int padding = (width > len) ? (width - len) : 0;
    if (!left_adjust) out_repeat(buf, left, fd, count, pad, padding);
    out_mem(buf, left, fd, count, s, (size_t)len);
    if (left_adjust) out_repeat(buf, left, fd, count, ' ', padding);
}
static int utoa_rev(char *tmp, unsigned long long v, unsigned base, int upper) {
    int n = 0;
    do {
        unsigned d = (unsigned)(v % base);
        tmp[n++] = (char)(d < 10 ? '0' + d : (upper ? 'A' : 'a') + d - 10);
        v /= base;
    } while (v);
    return n;
}
static void out_uint_fmt(char **buf, size_t *left, int fd, int *count,
                         unsigned long long v, unsigned base, char sign,
                         int upper, int width, int left_adjust, int zero_pad,
                         int precision, const char *prefix) {
    char digits[32];
    int ndigits = (precision == 0 && v == 0) ? 0 : utoa_rev(digits, v, base, upper);
    int prefix_len = prefix ? (int)strlen(prefix) : 0;
    int zeroes = (precision > ndigits) ? (precision - ndigits) : 0;
    int sign_len = sign ? 1 : 0;
    int total = sign_len + prefix_len + zeroes + ndigits;
    int pad_count = (width > total) ? (width - total) : 0;
    char pad = (zero_pad && !left_adjust && precision < 0) ? '0' : ' ';

    if (pad == '0') {
        if (sign) out_char(buf, left, fd, count, sign);
        if (prefix_len) out_mem(buf, left, fd, count, prefix, (size_t)prefix_len);
        out_repeat(buf, left, fd, count, '0', pad_count);
        out_repeat(buf, left, fd, count, '0', zeroes);
        while (ndigits-- > 0) out_char(buf, left, fd, count, digits[ndigits]);
        return;
    }

    if (!left_adjust) out_repeat(buf, left, fd, count, ' ', pad_count);
    if (sign) out_char(buf, left, fd, count, sign);
    if (prefix_len) out_mem(buf, left, fd, count, prefix, (size_t)prefix_len);
    out_repeat(buf, left, fd, count, '0', zeroes);
    while (ndigits-- > 0) out_char(buf, left, fd, count, digits[ndigits]);
    if (left_adjust) out_repeat(buf, left, fd, count, ' ', pad_count);
}

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    char *p = str;
    size_t left = size;
    int count = 0;
    while (*fmt) {
        if (*fmt != '%') { out_char(str ? &p : 0, &left, -1, &count, *fmt++); continue; }
        fmt++;
        int left_adjust = 0, zero_pad = 0, alt = 0, plus = 0, space = 0;
        int width = 0, precision = -1, length = 0;
        while (*fmt == '-' || *fmt == '+' || *fmt == ' ' || *fmt == '#' || *fmt == '0') {
            if (*fmt == '-') left_adjust = 1;
            else if (*fmt == '0') zero_pad = 1;
            else if (*fmt == '#') alt = 1;
            else if (*fmt == '+') plus = 1;
            else if (*fmt == ' ') space = 1;
            fmt++;
        }
        if (*fmt == '*') {
            width = va_arg(ap, int);
            if (width < 0) { left_adjust = 1; width = -width; }
            fmt++;
        } else {
            while (*fmt >= '0' && *fmt <= '9') {
                width = width * 10 + (*fmt++ - '0');
            }
        }
        if (*fmt == '.') {
            fmt++;
            precision = 0;
            if (*fmt == '*') {
                precision = va_arg(ap, int);
                fmt++;
            } else {
                while (*fmt >= '0' && *fmt <= '9') {
                    precision = precision * 10 + (*fmt++ - '0');
                }
            }
        }
        if (*fmt == 'l') {
            length = 1;
            fmt++;
            if (*fmt == 'l') { length = 2; fmt++; }
        } else if (*fmt == 'z') {
            length = 1;
            fmt++;
        }
        if (*fmt == 's') {
            const char *s = va_arg(ap, const char *);
            int len;
            if (!s) s = "(null)";
            len = (int)strlen(s);
            if (precision >= 0 && precision < len) len = precision;
            out_padded(str ? &p : 0, &left, -1, &count, s, len, width, left_adjust, ' ');
        }
        else if (*fmt == 'c') {
            char ch = (char)va_arg(ap, int);
            out_padded(str ? &p : 0, &left, -1, &count, &ch, 1, width, left_adjust, ' ');
        }
        else if (*fmt == 'd' || *fmt == 'i') {
            long long v = (length == 2) ? va_arg(ap, long long) :
                          (length == 1) ? va_arg(ap, long) : va_arg(ap, int);
            char sign = v < 0 ? '-' : (plus ? '+' : (space ? ' ' : 0));
            out_uint_fmt(str ? &p : 0, &left, -1, &count,
                         v < 0 ? (unsigned long long)-v : (unsigned long long)v,
                         10, sign, 0, width, left_adjust, zero_pad, precision, 0);
        }
        else if (*fmt == 'u' || *fmt == 'x' || *fmt == 'X' || *fmt == 'o') {
            unsigned base = (*fmt == 'o') ? 8u : ((*fmt == 'u') ? 10u : 16u);
            int upper = (*fmt == 'X');
            unsigned long long v = (length == 2) ? va_arg(ap, unsigned long long) :
                                   (length == 1) ? va_arg(ap, unsigned long) : va_arg(ap, unsigned);
            const char *prefix = 0;
            if (alt && v != 0 && (*fmt == 'x' || *fmt == 'X')) prefix = upper ? "0X" : "0x";
            else if (alt && v != 0 && *fmt == 'o') prefix = "0";
            out_uint_fmt(str ? &p : 0, &left, -1, &count, v, base, 0, upper,
                         width, left_adjust, zero_pad, precision, prefix);
        }
        else if (*fmt == 'p') {
            unsigned long v = (unsigned long)va_arg(ap, void *);
            out_uint_fmt(str ? &p : 0, &left, -1, &count, v, 16, 0, 0,
                         width ? width : 1, left_adjust, zero_pad, precision, "0x");
        }
        else if (*fmt == '%') out_char(str ? &p : 0, &left, -1, &count, '%');
        else out_char(str ? &p : 0, &left, -1, &count, *fmt);
        if (*fmt) fmt++;
    }
    if (str && size != 0) *p = '\0';
    return count;
}

int snprintf(char *str, size_t size, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int n = vsnprintf(str, size, fmt, ap); va_end(ap); return n; }
int sprintf(char *str, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int n = vsnprintf(str, 0xffffffffu, fmt, ap); va_end(ap); return n; }
int sscanf(const char *str, const char *fmt, ...) { (void)str; (void)fmt; return 0; }
int dprintf(int fd, const char *fmt, ...) { char buf[512]; va_list ap; va_start(ap, fmt); int n = vsnprintf(buf, sizeof(buf), fmt, ap); va_end(ap); (void)write(fd, buf, strlen(buf)); return n; }
int vfprintf(FILE *stream, const char *fmt, va_list ap) { char buf[512]; int n = vsnprintf(buf, sizeof(buf), fmt, ap); (void)write(file_fd(stream), buf, strlen(buf)); return n; }
int fprintf(FILE *stream, const char *fmt, ...) { va_list ap; va_start(ap, fmt); int n = vfprintf(stream, fmt, ap); va_end(ap); return n; }
int printf(const char *fmt, ...) { va_list ap; va_start(ap, fmt); int n = vfprintf(stdout, fmt, ap); va_end(ap); return n; }
int vasprintf(char **strp, const char *fmt, va_list ap) { int n = vsnprintf(0, 0, fmt, ap); if (n < 0) return -1; *strp = malloc((size_t)(n + 1)); if (!*strp) return -1; return vsnprintf(*strp, (size_t)(n + 1), fmt, ap); }

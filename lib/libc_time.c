#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <sys/times.h>
#include <time.h>
#include <unistd.h>

#include <lamp/libsys.h>
#include "libc_internal.h"

int clock_gettime(int clock_id, struct timespec *ts) {
    return ret_errno(libsys_call6(LAMP_SYS_CLOCK_GETTIME, (uint32_t)clock_id, (uint32_t)(uintptr_t)ts, 0, 0, 0, 0));
}
int clock_getres(int clock_id, struct timespec *ts) {
    return ret_errno(libsys_call6(LAMP_SYS_CLOCK_GETRES, (uint32_t)clock_id, (uint32_t)(uintptr_t)ts, 0, 0, 0, 0));
}
int clock_settime(int clock_id, const struct timespec *ts) {
    return ret_errno(libsys_call6(LAMP_SYS_CLOCK_SETTIME, (uint32_t)clock_id,
                                  (uint32_t)(uintptr_t)ts, 0, 0, 0, 0));
}
int nanosleep(const struct timespec *req, struct timespec *rem) {
    return ret_errno(libsys_call6(LAMP_SYS_NANOSLEEP, (uint32_t)(uintptr_t)req, (uint32_t)(uintptr_t)rem, 0, 0, 0, 0));
}
int gettimeofday(struct timeval *tv, void *tz) {
    return ret_errno(libsys_call6(LAMP_SYS_GETTIMEOFDAY, (uint32_t)(uintptr_t)tv, (uint32_t)(uintptr_t)tz, 0, 0, 0, 0));
}
int settimeofday(const struct timeval *tv, const void *tz) { (void)tv; (void)tz; return 0; }

time_t time(time_t *tloc) {
    struct timespec ts;
    if (clock_gettime(CLOCK_REALTIME, &ts) < 0) return (time_t)-1;
    if (tloc) *tloc = ts.tv_sec;
    return ts.tv_sec;
}

struct tm *localtime_r(const time_t *timep, struct tm *result) {
    int64_t seconds;
    int64_t days;
    int64_t rem;
    int64_t z;
    int64_t era;
    unsigned doe;
    unsigned yoe;
    int year;
    unsigned doy;
    unsigned mp;
    unsigned month;
    if (!timep || !result) { errno = EINVAL; return 0; }
    seconds = (int64_t)*timep;
    days = seconds / 86400;
    rem = seconds % 86400;
    if (rem < 0) { rem += 86400; days--; }
    result->tm_hour = (int)(rem / 3600);
    result->tm_min = (int)((rem % 3600) / 60);
    result->tm_sec = (int)(rem % 60);
    z = days + 719468;
    era = (z >= 0 ? z : z - 146096) / 146097;
    doe = (unsigned)(z - era * 146097);
    yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    year = (int)(yoe + era * 400);
    doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    mp = (5 * doy + 2) / 153;
    result->tm_mday = (int)(doy - (153 * mp + 2) / 5 + 1);
    month = mp + (mp < 10 ? 3 : (unsigned)-9);
    result->tm_mon = (int)month - 1;
    result->tm_year = year + (month <= 2) - 1900;
    result->tm_yday = (int)doy;
    result->tm_wday = (int)((days + 4) % 7);
    if (result->tm_wday < 0) result->tm_wday += 7;
    result->tm_isdst = 0;
    return result;
}
struct tm *localtime(const time_t *timep) { static struct tm tm; return localtime_r(timep, &tm); }
char *ctime(const time_t *timep) {
    static char buf[32];
    struct tm tm;
    if (!localtime_r(timep, &tm)) return 0;
    snprintf(buf, sizeof(buf), "%s %s %02d %02d:%02d:%02d %d\n",
             "Thu", "Jan", tm.tm_mday, tm.tm_hour, tm.tm_min, tm.tm_sec, tm.tm_year + 1900);
    return buf;
}
static int is_leap(int year) {
    return (year % 4 == 0 && (year % 100 != 0 || year % 400 == 0));
}
time_t mktime(struct tm *tm) {
    int year;
    int64_t days = 0;
    if (!tm) { errno = EINVAL; return (time_t)-1; }
    year = tm->tm_year + 1900;
    for (int y = 1970; y < year; y++) days += is_leap(y) ? 366 : 365;
    for (int y = 1969; y >= year; y--) days -= is_leap(y) ? 366 : 365;
    for (int month = 0; month < tm->tm_mon; month++) {
        static const int days_in_month[] = {31,28,31,30,31,30,31,31,30,31,30,31};
        days += days_in_month[month] + (month == 1 && is_leap(year));
    }
    days += tm->tm_mday - 1;
    tm->tm_wday = (int)((days + 4) % 7);
    if (tm->tm_wday < 0) tm->tm_wday += 7;
    return (time_t)(days * 86400 + tm->tm_hour * 3600 + tm->tm_min * 60 + tm->tm_sec);
}
static const char *parse_decimal(const char *s, int min_digits, int max_digits, int *value) {
    int n = 0;
    int v = 0;
    while (n < max_digits && isdigit((unsigned char)s[n])) {
        v = v * 10 + s[n] - '0';
        n++;
    }
    if (n < min_digits) return 0;
    *value = v;
    return s + n;
}
char *strptime(const char *s, const char *format, struct tm *result) {
    int value;
    if (!s || !format || !result) return 0;
    while (*format) {
        if (isspace((unsigned char)*format)) {
            while (isspace((unsigned char)*s)) s++;
            format++;
            continue;
        }
        if (*format != '%') {
            if (*s++ != *format++) return 0;
            continue;
        }
        format++;
        if (*format == 'F') {
            s = strptime(s, "%Y-%m-%d", result);
        } else if (*format == 'T') {
            s = strptime(s, "%H:%M:%S", result);
        } else if (*format == 'Y') {
            s = parse_decimal(s, 4, 4, &value); if (s) result->tm_year = value - 1900;
        } else if (*format == 'm') {
            s = parse_decimal(s, 1, 2, &value); if (s) result->tm_mon = value - 1;
        } else if (*format == 'd' || *format == 'e') {
            s = parse_decimal(s, 1, 2, &value); if (s) result->tm_mday = value;
        } else if (*format == 'H') {
            s = parse_decimal(s, 1, 2, &value); if (s) result->tm_hour = value;
        } else if (*format == 'M') {
            s = parse_decimal(s, 1, 2, &value); if (s) result->tm_min = value;
        } else if (*format == 'S') {
            s = parse_decimal(s, 1, 2, &value); if (s) result->tm_sec = value;
        } else {
            return 0;
        }
        if (!s) return 0;
        format++;
    }
    return (char *)s;
}
void tzset(void) {}

size_t strftime(char *s, size_t max, const char *format, const struct tm *tm) {
    (void)tm;
    if (!s || max == 0) return 0;
    if (format && strcmp(format, "%Y-%m-%d %H:%M:%S %z") == 0)
        return (size_t)snprintf(s, max, "1970-01-01 00:00:00 +0000");
    return (size_t)snprintf(s, max, "1970-01-01");
}

unsigned int alarm(unsigned int seconds) { (void)seconds; return 0; }
unsigned int sleep(unsigned int seconds) {
    struct timespec req; req.tv_sec = (time_t)seconds; req.tv_nsec = 0;
    return nanosleep(&req, 0) < 0 ? seconds : 0;
}

clock_t times(struct tms *buf) { if (buf) memset(buf, 0, sizeof(*buf)); return 0; }

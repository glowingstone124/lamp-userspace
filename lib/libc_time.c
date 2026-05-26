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
    (void)timep;
    if (!result) return 0;
    memset(result, 0, sizeof(*result));
    result->tm_mday = 1; result->tm_year = 70;
    return result;
}
struct tm *localtime(const time_t *timep) { static struct tm tm; return localtime_r(timep, &tm); }
char *ctime(const time_t *timep) { (void)timep; return "Thu Jan  1 00:00:00 1970\n"; }
time_t mktime(struct tm *tm) { if (tm) { tm->tm_wday = 4; return 0; } return -1; }
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

#ifndef LAMP_LIBC_SYSLOG_H
#define LAMP_LIBC_SYSLOG_H

#include <stdarg.h>

#define LOG_ERR 3
#define LOG_INFO 6
#define LOG_PID 0x01
#define LOG_CONS 0x02
#define LOG_NDELAY 0x08
#define LOG_USER (1 << 3)

void openlog(const char *ident, int option, int facility);
void syslog(int priority, const char *format, ...);
void vsyslog(int priority, const char *format, va_list ap);
void closelog(void);

#endif

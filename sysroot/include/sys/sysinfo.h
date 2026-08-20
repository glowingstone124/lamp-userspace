#ifndef LAMP_LIBC_SYS_SYSINFO_H
#define LAMP_LIBC_SYS_SYSINFO_H

struct sysinfo {
    long uptime;
    unsigned long loads[3];
    unsigned long totalram;
    unsigned long freeram;
    unsigned long sharedram;
    unsigned long bufferram;
    unsigned long totalswap;
    unsigned long freeswap;
    unsigned short procs;
    unsigned short pad;
    unsigned long totalhigh;
    unsigned long freehigh;
    unsigned int mem_unit;
    char _f[8];
};
int sysinfo(struct sysinfo *info);

#endif

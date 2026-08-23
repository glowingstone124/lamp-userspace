/*
Copyright 2026 Lamp VM contributors

CoreMark(R) platform port for the LAMP userspace ABI.
*/
#include <stdio.h>
#include <time.h>

#include "coremark.h"

static CORE_TICKS start_time_val;
static CORE_TICKS stop_time_val;

/* The benchmark is timed against the VM's paced monotonic clock.  A 32-bit
 * millisecond counter is sufficient for one CoreMark invocation and avoids
 * requiring 64-bit division in the timing path. */
static CORE_TICKS monotonic_millis(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) {
        return 0;
    }
    return (CORE_TICKS)((uint32_t)ts.tv_sec * 1000u
                        + (uint32_t)ts.tv_nsec / 1000000u);
}

void start_time(void)
{
    start_time_val = monotonic_millis();
}

void stop_time(void)
{
    stop_time_val = monotonic_millis();
}

CORE_TICKS get_time(void)
{
    return stop_time_val - start_time_val;
}

secs_ret time_in_secs(CORE_TICKS ticks)
{
    return ticks / 1000u;
}

void *portable_malloc(ee_size_t size)
{
    (void)size;
    return NULL;
}

void portable_free(void *p)
{
    (void)p;
}

ee_u32 default_num_contexts = 1;

void portable_init(core_portable *p, int *argc, char *argv[])
{
    (void)argc;
    (void)argv;

    if (sizeof(ee_ptr_int) != sizeof(ee_u8 *)) {
        ee_printf("ERROR! Please define ee_ptr_int to hold a pointer!\n");
    }
    if (sizeof(ee_u32) != 4) {
        ee_printf("ERROR! Please define ee_u32 to a 32b unsigned type!\n");
    }
    p->portable_id = 1;
}

void portable_fini(core_portable *p)
{
    p->portable_id = 0;
}

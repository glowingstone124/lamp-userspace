/*
Copyright 2026 Lamp VM contributors

CoreMark(R) platform port for the LAMP userspace ABI.  The benchmark sources
under ../../coremark are maintained unchanged from the EEMBC distribution.
*/
#ifndef LAMP_CORE_PORTME_H
#define LAMP_CORE_PORTME_H

#include <stddef.h>
#include <stdint.h>

/* LAMP exposes a monotonic POSIX clock and a printf implementation, but its
 * ISA has only binary32 floating point instructions.  Use CoreMark's
 * integer-time output mode so reporting does not require double arithmetic.
 */
#define HAS_FLOAT  0
#define HAS_TIME_H 1
#define USE_CLOCK  0
#define HAS_STDIO  1
#define HAS_PRINTF 1

typedef uint32_t CORE_TICKS;

#define COMPILER_VERSION "LAMP LLVM 23"
#ifndef COMPILER_FLAGS
#define COMPILER_FLAGS   "-O2 -ffreestanding -fno-builtin -fno-stack-protector"
#endif
#define MEM_LOCATION     "Static RAM"

typedef signed short   ee_s16;
typedef unsigned short ee_u16;
typedef signed int     ee_s32;
typedef double         ee_f32;
typedef unsigned char  ee_u8;
typedef unsigned int   ee_u32;
typedef uintptr_t      ee_ptr_int;
typedef size_t         ee_size_t;

#define align_mem(x) (void *)(4 + (((ee_ptr_int)(x) - 1) & ~3u))

/* The guest CRT passes argc/argv, so users can supply the standard CoreMark
 * seed and iteration arguments. */
#define SEED_METHOD SEED_ARG
#define MEM_METHOD  MEM_STATIC

#define MULTITHREAD 1
#define USE_PTHREAD 0
#define USE_FORK    0
#define USE_SOCKET  0

#define MAIN_HAS_NOARGC    0
#define MAIN_HAS_NORETURN  0

extern ee_u32 default_num_contexts;

typedef struct CORE_PORTABLE_S {
    ee_u8 portable_id;
} core_portable;

void portable_init(core_portable *p, int *argc, char *argv[]);
void portable_fini(core_portable *p);

#if !defined(PROFILE_RUN) && !defined(PERFORMANCE_RUN) && !defined(VALIDATION_RUN)
#if (TOTAL_DATA_SIZE == 1200)
#define PROFILE_RUN 1
#elif (TOTAL_DATA_SIZE == 2000)
#define PERFORMANCE_RUN 1
#else
#define VALIDATION_RUN 1
#endif
#endif

#endif /* LAMP_CORE_PORTME_H */

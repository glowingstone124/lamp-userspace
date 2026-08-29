#include <stdio.h>

/*
 * Deliberately trigger the host VM's unaligned-access panic path.
 * This is a destructive diagnostic: after it runs, the VM must be restarted.
 */
int main(void) {
    puts("vm-panic-test: issuing an intentionally unaligned LOAD16");
    __asm__ volatile(
        "movi r1, 1\n"
        "load16 r0, r1, 0\n"
        :
        :
        : "r0", "r1", "memory");
    __builtin_unreachable();
}

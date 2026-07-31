#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <lamp/libsys.h>
#include "libc_internal.h"

int sigaction(int sig, const struct sigaction *act, struct sigaction *oldact) {
    lamp_sigaction_t la, old;
    lamp_sigaction_t *lap = 0, *oldp = oldact ? &old : 0;
    if (act) {
        la.handler = (uint32_t)(uintptr_t)act->sa_handler;
        la.flags = (uint32_t)act->sa_flags;
        la.mask = act->sa_mask;
        la.restorer = (uint32_t)(uintptr_t)act->sa_restorer;
        if (la.handler > LAMP_SIG_IGN && la.restorer == 0u) {
            la.restorer = (uint32_t)(uintptr_t)&__lamp_signal_restorer;
        }
        lap = &la;
    }
    if (libsys_sigaction((uint32_t)sig, lap, oldp) < 0) { set_errno_from_libsys(); return -1; }
    if (oldact) {
        oldact->sa_handler = (sighandler_t)(uintptr_t)old.handler;
        oldact->sa_flags = (int)old.flags;
        oldact->sa_mask = old.mask;
        oldact->sa_restorer = (void (*)(void))(uintptr_t)old.restorer;
    }
    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) { return ret_errno(libsys_sigprocmask((uint32_t)how, set, oldset)); }
int kill(pid_t pid, int sig) { return ret_errno(libsys_kill(pid, (uint32_t)sig)); }

int sigemptyset(sigset_t *set)  { *set = 0; return 0; }
int sigfillset(sigset_t *set)   { *set = 0xffffffffu; return 0; }
int sigaddset(sigset_t *set, int sig)    { if (sig > 0 && sig <= 32) *set |= 1u << (sig - 1); return 0; }
int sigdelset(sigset_t *set, int sig)    { if (sig > 0 && sig <= 32) *set &= ~(1u << (sig - 1)); return 0; }
int sigismember(const sigset_t *set, int sig) { if (sig > 0 && sig <= 32 && set) return !!(*set & (1u << (sig - 1))); return 0; }
int sigisemptyset(const sigset_t *set)   { return set ? *set == 0 : 1; }

sighandler_t signal(int sig, sighandler_t handler) {
    struct sigaction act, old;
    act.sa_handler = handler; act.sa_mask = 0; act.sa_flags = SA_RESTART; act.sa_restorer = 0;
    if (sigaction(sig, &act, &old) < 0) return SIG_ERR;
    return old.sa_handler;
}

int sigsuspend(const sigset_t *mask) { (void)mask; errno = ENOSYS; return -1; }
int raise(int sig) { return kill(getpid(), sig); }
char *strsignal(int sig) { (void)sig; return "Unknown signal"; }

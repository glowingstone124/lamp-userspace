#include <signal.h>
#include <unistd.h>

static volatile int g_seen;

static void signal_handler(int sig) {
    g_seen = sig;
}

int main(void) {
    static const char pass[] = "signal-delivery: PASS\n";
    static const char fail[] = "signal-delivery: FAIL\n";
    struct sigaction action;
    sigset_t set;

    action.sa_handler = signal_handler;
    action.sa_mask = 0u;
    action.sa_flags = 0;
    action.sa_restorer = 0;
    if (sigaction(SIGUSR1, &action, 0) != 0) {
        (void)write(1, fail, sizeof(fail) - 1u);
        return 1;
    }

    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    if (sigprocmask(SIG_BLOCK, &set, 0) != 0 || raise(SIGUSR1) != 0 || g_seen != 0) {
        (void)write(1, fail, sizeof(fail) - 1u);
        return 1;
    }
    if (sigprocmask(SIG_UNBLOCK, &set, 0) != 0 || g_seen != SIGUSR1) {
        (void)write(1, fail, sizeof(fail) - 1u);
        return 1;
    }

    g_seen = 0;
    if (raise(SIGUSR1) != 0 || g_seen != SIGUSR1) {
        (void)write(1, fail, sizeof(fail) - 1u);
        return 1;
    }

    (void)write(1, pass, sizeof(pass) - 1u);
    return 0;
}

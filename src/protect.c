/* ----------------------------------------------------------------------------
 * Copyright (c) 2013 Diogo Behrens
 * Distributed under the MIT license. See accompanying file LICENSE.
 * ------------------------------------------------------------------------- */
#include <stdint.h>
#include <sys/mman.h> // mprotect
#include <errno.h>    // perror
#include <stdlib.h>   // exit
#include <signal.h>

#include "debug.h"
#include "protect.h"
static void protect_handler(int sig, siginfo_t* si, void* args);

void
protect_setsignal()
{
#ifdef SEI_SIGSEGV_RECOVERY
    /* Set up alternate signal stack for SIGSEGV recovery
     * Use fixed size (64KB) instead of SIGSTKSZ which may not be a constant */
    #define SEI_ALTSTACK_SIZE (64 * 1024)
    static __thread char altstack[SEI_ALTSTACK_SIZE];
    stack_t ss;
    ss.ss_sp = altstack;
    ss.ss_size = SEI_ALTSTACK_SIZE;
    ss.ss_flags = 0;
    if (sigaltstack(&ss, NULL) == -1) {
        perror("sigaltstack");
    }
#endif

    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
#ifdef SEI_SIGSEGV_RECOVERY
    sa.sa_flags |= SA_ONSTACK;  /* Use alternate stack */
#endif
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = protect_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit (EXIT_FAILURE);
    }
}

#ifdef SEI_SIGSEGV_RECOVERY
/* ----------------------------------------------------------------------------
 * SIGSEGV Recovery Handler
 * Recovers from SIGSEGV within transactions by rollback and retry
 * Terminates when all cores are blacklisted (handled by cpu_isolation)
 * ------------------------------------------------------------------------- */

#define PROTECT_HANDLER                                                 \
    void protect_handler(int sig, siginfo_t* si, void* args)            \
    {                                                                   \
        /* 1. Outside transaction: unrecoverable */                     \
        if (!__sei_thread || !__sei_thread->sei ||                      \
            sei_getp(__sei_thread->sei) < 0) {                          \
            signal(SIGSEGV, SIG_DFL);                                   \
            raise(SIGSEGV);                                             \
            return;                                                     \
        }                                                               \
                                                                        \
        /* 2. SIGSEGV within transaction: recovery */                   \
                                                                        \
        /* 3. Blacklist current core and migrate                        \
         * cpu_isolation_migrate_current_thread() exits if              \
         * all cores are blacklisted */                                 \
        sei_setp(__sei_thread->sei, -1);                                \
        cpu_isolation_blacklist_current();                              \
        cpu_isolation_migrate_current_thread();                         \
                                                                        \
        /* 4. Rollback transaction state */                             \
        sei_rollback(__sei_thread->sei);                                \
                                                                        \
        /* 5. Retry from Phase 0 */                                     \
        __sei_switch(&__sei_thread->ctx, 0x00);                         \
    }

#elif defined(HEAP_PROTECT)
/* ----------------------------------------------------------------------------
 * Standard PROTECT_HANDLER (HEAP_PROTECT only, no SIGSEGV recovery)
 * ------------------------------------------------------------------------- */
#define PROTECT_HANDLER                                                 \
    void protect_handler(int sig, siginfo_t* si, void* args)            \
    {                                                                   \
        sei_unprotect(__sei_thread->sei, si->si_addr, 1);               \
    }

#else
/* ----------------------------------------------------------------------------
 * Standard PROTECT_HANDLER (no SIGSEGV recovery)
 * ------------------------------------------------------------------------- */
#define PROTECT_HANDLER                                                 \
    void protect_handler(int sig, siginfo_t* si, void* args)            \
    {                                                                   \
        sei_unprotect(__sei_thread->sei, si->si_addr, 1);               \
    }
#endif /* SEI_SIGSEGV_RECOVERY */


void
protect_mem(void* addr, size_t size, protect_t prot)
{
    assert (addr && "invalid address");
    assert (size > 0);

    // align address to page
    uintptr_t uaddr = (uintptr_t) addr;
    uintptr_t paddr = uaddr & ~(4096 - 1);
    size += uaddr - paddr;

    int p = PROT_READ | (prot == WRITE ? PROT_WRITE : 0);

    if (mprotect((void *) paddr, size, p)) {
        perror("mprotect");
        //exit (EXIT_FAILURE);
        assert (0);
    }
}

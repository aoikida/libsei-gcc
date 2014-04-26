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
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sigemptyset(&sa.sa_mask);
    sa.sa_sigaction = protect_handler;
    if (sigaction(SIGSEGV, &sa, NULL) == -1) {
        perror("sigaction");
        exit (EXIT_FAILURE);
    }
}

//printf("si->si_addr = %p (p=%d)\n", si->si_addr, __tmasco.asco->p);

#define PROTECT_HANDLER                                                 \
    void protect_handler(int sig, siginfo_t* si, void* args)            \
    {                                                                   \
        asco_unprotect(__tmasco->asco, si->si_addr, 1);                 \
    }


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

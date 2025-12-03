/* -----------------------------------------------------------------------------
 * CPU Isolation Manager for libsei
 * Provides CPU core blacklisting and thread migration for SDC recovery
 * -------------------------------------------------------------------------- */

#define _GNU_SOURCE
#include <sched.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include "cpu_isolation.h"

#ifdef SEI_CPU_ISOLATION

/* Global CPU isolation state */
cpu_isolation_state_t cpu_isolation_state;

/* --- Initialization --- */

int cpu_isolation_init(void) {
    memset(&cpu_isolation_state, 0, sizeof(cpu_isolation_state_t));

    /* Get number of available CPU cores */
    cpu_isolation_state.num_cores = (int)sysconf(_SC_NPROCESSORS_ONLN);
    if (cpu_isolation_state.num_cores <= 0) {
        fprintf(stderr, "cpu_isolation_init: failed to get CPU count\n");
        return -1;
    }

    /* Initialize available_cores bitmask (all cores initially available) */
    if (cpu_isolation_state.num_cores >= 64) {
        fprintf(stderr, "cpu_isolation_init: too many cores (%d), max 64\n",
                cpu_isolation_state.num_cores);
        return -1;
    }
    cpu_isolation_state.available_cores = (1ULL << cpu_isolation_state.num_cores) - 1;

    /* Initialize blacklist (no cores blacklisted initially) */
    cpu_isolation_state.blacklist = 0;
    cpu_isolation_state.num_blacklisted = 0;

    /* Initialize mutex */
    if (pthread_mutex_init(&cpu_isolation_state.lock, NULL) != 0) {
        fprintf(stderr, "cpu_isolation_init: mutex init failed\n");
        return -1;
    }

    /* Initialize statistics */
    cpu_isolation_state.migration_count = 0;
    cpu_isolation_state.blacklist_events = 0;

    fprintf(stderr, "cpu_isolation_init: initialized with %d cores\n",
            cpu_isolation_state.num_cores);

    return 0;
}

void cpu_isolation_cleanup(void) {
    pthread_mutex_destroy(&cpu_isolation_state.lock);
}

/* --- Core Management --- */

int cpu_isolation_blacklist_current(void) {
    int core_id = sched_getcpu();
    if (core_id < 0) {
        fprintf(stderr, "cpu_isolation_blacklist_current: sched_getcpu failed\n");
        return -1;
    }

    pthread_mutex_lock(&cpu_isolation_state.lock);

    /* Check if already blacklisted */
    uint64_t core_mask = 1ULL << core_id;
    if (cpu_isolation_state.blacklist & core_mask) {
        pthread_mutex_unlock(&cpu_isolation_state.lock);
        return core_id; /* Already blacklisted */
    }

    /* Blacklist the core */
    cpu_isolation_state.blacklist |= core_mask;
    cpu_isolation_state.num_blacklisted++;
    cpu_isolation_state.blacklist_events++;

    fprintf(stderr, "cpu_isolation: blacklisted core %d (%d/%d cores blacklisted)\n",
            core_id, cpu_isolation_state.num_blacklisted, cpu_isolation_state.num_cores);

    pthread_mutex_unlock(&cpu_isolation_state.lock);

    return core_id;
}

int cpu_isolation_is_blacklisted(int core_id) {
    if (core_id < 0 || core_id >= cpu_isolation_state.num_cores) {
        return 1; /* Invalid core_id treated as blacklisted */
    }

    pthread_mutex_lock(&cpu_isolation_state.lock);
    uint64_t core_mask = 1ULL << core_id;
    int is_blacklisted = (cpu_isolation_state.blacklist & core_mask) ? 1 : 0;
    pthread_mutex_unlock(&cpu_isolation_state.lock);

    return is_blacklisted;
}

int cpu_isolation_get_available_count(void) {
    pthread_mutex_lock(&cpu_isolation_state.lock);
    int count = cpu_isolation_state.num_cores - cpu_isolation_state.num_blacklisted;
    pthread_mutex_unlock(&cpu_isolation_state.lock);
    return count;
}

/* --- Thread Migration --- */

int cpu_isolation_get_next_available(void) {
    /* Caller must hold lock */
    uint64_t available_mask = cpu_isolation_state.available_cores & ~cpu_isolation_state.blacklist;

    if (available_mask == 0) {
        return -1; /* All cores blacklisted */
    }

    /* Find first available core (lowest bit set) */
    for (int i = 0; i < cpu_isolation_state.num_cores; i++) {
        if (available_mask & (1ULL << i)) {
            return i;
        }
    }

    return -1; /* Should not reach here */
}

int cpu_isolation_migrate_current_thread(void) {
    pthread_mutex_lock(&cpu_isolation_state.lock);

    /* Check if all cores are blacklisted */
    if (cpu_isolation_state.num_blacklisted >= cpu_isolation_state.num_cores) {
        pthread_mutex_unlock(&cpu_isolation_state.lock);
        fprintf(stderr, "cpu_isolation: all cores blacklisted, exiting process\n");
        cpu_isolation_print_stats();
        exit(EXIT_FAILURE);
    }

    /* Get next available core */
    int new_core = cpu_isolation_get_next_available();
    if (new_core < 0) {
        pthread_mutex_unlock(&cpu_isolation_state.lock);
        fprintf(stderr, "cpu_isolation: no available cores, exiting process\n");
        cpu_isolation_print_stats();
        exit(EXIT_FAILURE);
    }

    /* Set CPU affinity to the new core */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(new_core, &cpuset);

    pthread_t current_thread = pthread_self();
    int ret = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);

    if (ret != 0) {
        pthread_mutex_unlock(&cpu_isolation_state.lock);
        fprintf(stderr, "cpu_isolation: pthread_setaffinity_np failed: %s\n", strerror(ret));
        fprintf(stderr, "cpu_isolation: migration failed, exiting process\n");
        exit(EXIT_FAILURE);
    }

    cpu_isolation_state.migration_count++;

    fprintf(stderr, "cpu_isolation: migrated thread to core %d\n", new_core);

    pthread_mutex_unlock(&cpu_isolation_state.lock);

#ifdef DEBUG
    /* Verify migration (debug only - may have false positives due to scheduler race) */
    int actual_core = sched_getcpu();
    if (actual_core != new_core) {
        fprintf(stderr, "cpu_isolation: migration pending (expected %d, currently on %d)\n",
                new_core, actual_core);
    }
#endif

    return new_core;
}

int cpu_isolation_set_affinity(pthread_t thread) {
    pthread_mutex_lock(&cpu_isolation_state.lock);

    /* Get next available core */
    int core = cpu_isolation_get_next_available();
    if (core < 0) {
        pthread_mutex_unlock(&cpu_isolation_state.lock);
        fprintf(stderr, "cpu_isolation_set_affinity: no available cores\n");
        return -1;
    }

    /* Set CPU affinity */
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(core, &cpuset);

    int ret = pthread_setaffinity_np(thread, sizeof(cpu_set_t), &cpuset);

    pthread_mutex_unlock(&cpu_isolation_state.lock);

    if (ret != 0) {
        fprintf(stderr, "cpu_isolation_set_affinity: pthread_setaffinity_np failed: %s\n",
                strerror(ret));
        return -1;
    }

    return 0;
}

/* --- Statistics --- */

const cpu_isolation_state_t* cpu_isolation_get_stats(void) {
    return &cpu_isolation_state;
}

void cpu_isolation_print_stats(void) {
    pthread_mutex_lock(&cpu_isolation_state.lock);

    fprintf(stderr, "\n=== CPU Isolation Statistics ===\n");
    fprintf(stderr, "Total cores: %d\n", cpu_isolation_state.num_cores);
    fprintf(stderr, "Blacklisted cores: %d\n", cpu_isolation_state.num_blacklisted);
    fprintf(stderr, "Available cores: %d\n",
            cpu_isolation_state.num_cores - cpu_isolation_state.num_blacklisted);
    fprintf(stderr, "Blacklist events: %lu\n", cpu_isolation_state.blacklist_events);
    fprintf(stderr, "Thread migrations: %lu\n", cpu_isolation_state.migration_count);
    fprintf(stderr, "Blacklist bitmask: 0x%016lx\n", cpu_isolation_state.blacklist);
    fprintf(stderr, "================================\n\n");

    pthread_mutex_unlock(&cpu_isolation_state.lock);
}

#endif /* SEI_CPU_ISOLATION */

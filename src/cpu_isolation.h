/* -----------------------------------------------------------------------------
 * CPU Isolation Manager for libsei
 * Provides CPU core blacklisting and thread migration for SDC recovery
 * -------------------------------------------------------------------------- */

#ifndef CPU_ISOLATION_H
#define CPU_ISOLATION_H

#include <stdint.h>
#include <pthread.h>

#ifdef SEI_CPU_ISOLATION

/* CPU Isolation Manager state */
typedef struct {
    __uint128_t blacklist;       /* Bitmask of blacklisted cores (1 = blacklisted) */
    __uint128_t available_cores; /* Bitmask of initially available cores */
    int num_cores;               /* Total number of CPU cores */
    int num_blacklisted;         /* Count of blacklisted cores */
    pthread_mutex_t lock;        /* Mutex for thread-safe operations */

    /* Statistics */
    uint64_t migration_count;    /* Total number of thread migrations */
    uint64_t blacklist_events;   /* Total number of blacklist events */
} cpu_isolation_state_t;

/* Global CPU isolation state */
extern cpu_isolation_state_t cpu_isolation_state;

/* --- Initialization --- */

/**
 * Initialize CPU Isolation Manager
 * Must be called once at process startup
 * Returns: 0 on success, -1 on failure
 */
int cpu_isolation_init(void);

/**
 * Cleanup CPU Isolation Manager
 * Should be called at process shutdown
 */
void cpu_isolation_cleanup(void);

/* --- Core Management --- */

/**
 * Blacklist the CPU core on which the current thread is running
 * Thread-safe operation
 * Returns: core_id that was blacklisted, -1 on failure
 */
int cpu_isolation_blacklist_current(void);

/**
 * Blacklist a specific CPU core by core_id
 * Thread-safe operation
 * core_id: CPU core ID to blacklist
 */
void cpu_isolation_blacklist_core(int core_id);

/**
 * Check if a specific core is blacklisted
 * Returns: 1 if blacklisted, 0 if available
 */
int cpu_isolation_is_blacklisted(int core_id);

/**
 * Get the number of available (non-blacklisted) cores
 * Returns: count of available cores
 */
int cpu_isolation_get_available_count(void);

/* --- Thread Migration --- */

/**
 * Migrate current thread to an available (non-blacklisted) core
 * If all cores are blacklisted, calls exit(EXIT_FAILURE)
 * Thread-safe operation
 * Returns: new core_id, does not return if all cores blacklisted
 */
int cpu_isolation_migrate_current_thread(void);

/**
 * Migrate current thread to an available core, excluding a specific core
 * exclude_core: Core ID to exclude from selection (-1 to disable exclusion)
 * Thread-safe operation
 * Returns: new core_id, does not return if no suitable cores available
 */
int cpu_isolation_migrate_excluding_core(int exclude_core);

/**
 * Set CPU affinity for a new thread (called from pthread wrapper)
 * Assigns thread to a non-blacklisted core
 * Returns: 0 on success, -1 on failure
 */
int cpu_isolation_set_affinity(pthread_t thread);

/* --- Statistics --- */

/**
 * Get current CPU isolation statistics
 * Returns pointer to internal state (read-only)
 */
const cpu_isolation_state_t* cpu_isolation_get_stats(void);

/**
 * Print CPU isolation statistics to stderr
 * For debugging and monitoring
 */
void cpu_isolation_print_stats(void);

/* --- Internal Helpers --- */

/**
 * Get the next available (non-blacklisted) core
 * Returns: core_id, or -1 if all cores are blacklisted
 */
int cpu_isolation_get_next_available(void);

#endif /* SEI_CPU_ISOLATION */

#endif /* CPU_ISOLATION_H */

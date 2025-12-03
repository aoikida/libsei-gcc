/* -----------------------------------------------------------------------------
 * CPU Isolation Manager Unit Tests
 * -------------------------------------------------------------------------- */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sched.h>
#include <unistd.h>

/* Define SEI_CPU_ISOLATION to enable CPU isolation code */
#define SEI_CPU_ISOLATION
#include "cpu_isolation.h"

/* Test result tracking */
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) \
    static void test_##name(void); \
    static void run_test_##name(void) { \
        printf("Running test: %s\n", #name); \
        tests_run++; \
        test_##name(); \
        tests_passed++; \
        printf("  PASS\n"); \
    } \
    static void test_##name(void)

#define ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            fprintf(stderr, "  FAIL: %s (line %d): %s\n", __func__, __LINE__, message); \
            tests_failed++; \
            tests_passed--; \
            return; \
        } \
    } while (0)

/* --- Test T001: Initialization Test --- */
TEST(init_basic) {
    int ret = cpu_isolation_init();
    ASSERT(ret == 0, "Initialization should succeed");

    const cpu_isolation_state_t *state = cpu_isolation_get_stats();
    ASSERT(state->num_cores > 0, "Should detect CPU cores");
    ASSERT(state->num_blacklisted == 0, "No cores should be blacklisted initially");
    ASSERT(state->blacklist == 0, "Blacklist bitmask should be zero");

    int available = cpu_isolation_get_available_count();
    ASSERT(available == state->num_cores, "All cores should be available initially");

    cpu_isolation_cleanup();
}

/* --- Test T002: Basic Blacklisting --- */
TEST(blacklist_current) {
    cpu_isolation_init();

    int initial_core = sched_getcpu();
    ASSERT(initial_core >= 0, "Should be able to get current CPU");

    int blacklisted_core = cpu_isolation_blacklist_current();
    ASSERT(blacklisted_core == initial_core, "Should blacklist current core");

    int is_blacklisted = cpu_isolation_is_blacklisted(blacklisted_core);
    ASSERT(is_blacklisted == 1, "Core should be marked as blacklisted");

    const cpu_isolation_state_t *state = cpu_isolation_get_stats();
    ASSERT(state->num_blacklisted == 1, "One core should be blacklisted");
    ASSERT(state->blacklist_events == 1, "Should record one blacklist event");

    cpu_isolation_cleanup();
}

/* --- Test T003: Double Blacklisting --- */
TEST(blacklist_idempotent) {
    cpu_isolation_init();

    int core1 = cpu_isolation_blacklist_current();
    int core2 = cpu_isolation_blacklist_current();

    ASSERT(core1 == core2, "Should return same core on double blacklist");

    const cpu_isolation_state_t *state = cpu_isolation_get_stats();
    ASSERT(state->num_blacklisted == 1, "Should still have only one blacklisted core");

    cpu_isolation_cleanup();
}

/* --- Test T004: Available Count --- */
TEST(available_count) {
    cpu_isolation_init();

    const cpu_isolation_state_t *state = cpu_isolation_get_stats();
    int initial_count = state->num_cores;
    int available = cpu_isolation_get_available_count();

    ASSERT(available == initial_count, "Initially all cores available");

    cpu_isolation_blacklist_current();
    available = cpu_isolation_get_available_count();

    ASSERT(available == initial_count - 1, "Should have one less available core");

    cpu_isolation_cleanup();
}

/* --- Test T005: Is Blacklisted Check --- */
TEST(is_blacklisted_check) {
    cpu_isolation_init();

    int current_core = sched_getcpu();

    /* Before blacklisting */
    int is_bl = cpu_isolation_is_blacklisted(current_core);
    ASSERT(is_bl == 0, "Core should not be blacklisted initially");

    /* After blacklisting */
    cpu_isolation_blacklist_current();
    is_bl = cpu_isolation_is_blacklisted(current_core);
    ASSERT(is_bl == 1, "Core should be blacklisted after blacklist call");

    cpu_isolation_cleanup();
}

/* --- Test T006: Statistics Tracking --- */
TEST(statistics_tracking) {
    cpu_isolation_init();

    const cpu_isolation_state_t *state = cpu_isolation_get_stats();

    ASSERT(state->blacklist_events == 0, "No blacklist events initially");
    ASSERT(state->migration_count == 0, "No migrations initially");

    cpu_isolation_blacklist_current();
    state = cpu_isolation_get_stats();

    ASSERT(state->blacklist_events == 1, "Should record blacklist event");

    cpu_isolation_cleanup();
}

/* --- Test T007: Print Statistics --- */
TEST(print_stats) {
    cpu_isolation_init();

    cpu_isolation_blacklist_current();

    printf("  Testing print_stats (output below):\n");
    cpu_isolation_print_stats();

    /* No assertion, just verify it doesn't crash */

    cpu_isolation_cleanup();
}

/* --- Test T101: Invalid Core Check --- */
TEST(invalid_core_check) {
    cpu_isolation_init();

    int is_bl = cpu_isolation_is_blacklisted(-1);
    ASSERT(is_bl == 1, "Invalid negative core should be treated as blacklisted");

    const cpu_isolation_state_t *state = cpu_isolation_get_stats();
    is_bl = cpu_isolation_is_blacklisted(state->num_cores + 10);
    ASSERT(is_bl == 1, "Out-of-range core should be treated as blacklisted");

    cpu_isolation_cleanup();
}

/* --- Test T201: Boundary - Single Core System --- */
TEST(single_core_boundary) {
    /* This test simulates single-core system behavior
     * We can't actually change the number of cores, so we just verify logic */

    cpu_isolation_init();

    const cpu_isolation_state_t *state = cpu_isolation_get_stats();

    if (state->num_cores == 1) {
        printf("  Running on actual single-core system\n");

        cpu_isolation_blacklist_current();
        int available = cpu_isolation_get_available_count();

        ASSERT(available == 0, "No cores should be available after blacklisting the only core");
    } else {
        printf("  Skipping single-core test (system has %d cores)\n", state->num_cores);
    }

    cpu_isolation_cleanup();
}

/* --- Test T202: Boundary - Maximum Cores --- */
TEST(max_cores_boundary) {
    cpu_isolation_init();

    const cpu_isolation_state_t *state = cpu_isolation_get_stats();

    ASSERT(state->num_cores < 64, "Should support up to 64 cores");
    ASSERT(state->available_cores == ((1ULL << state->num_cores) - 1),
           "All cores should be available in bitmask");

    cpu_isolation_cleanup();
}

/* --- Test T010: Thread Migration --- */
TEST(migration_basic) {
    cpu_isolation_init();

    const cpu_isolation_state_t *state = cpu_isolation_get_stats();

    if (state->num_cores <= 1) {
        printf("  Skipping migration test (need at least 2 cores)\n");
        cpu_isolation_cleanup();
        return;
    }

    int original_core = sched_getcpu();
    ASSERT(original_core >= 0, "Should get current core");

    /* Blacklist current core */
    cpu_isolation_blacklist_current();

    /* Migrate to another core */
    int new_core = cpu_isolation_migrate_current_thread();

    ASSERT(new_core >= 0, "Should migrate to a valid core");
    ASSERT(new_core != original_core, "Should migrate to a different core");

    state = cpu_isolation_get_stats();
    ASSERT(state->migration_count == 1, "Should record one migration");

    /* Verify we're actually on the new core */
    int actual_core = sched_getcpu();
    ASSERT(actual_core == new_core, "Should actually be on the new core");

    cpu_isolation_cleanup();
}

/* --- Main Test Runner --- */
int main(void) {
    printf("=== CPU Isolation Manager Unit Tests ===\n\n");

    /* T001-T007: Normal operation tests */
    run_test_init_basic();
    run_test_blacklist_current();
    run_test_blacklist_idempotent();
    run_test_available_count();
    run_test_is_blacklisted_check();
    run_test_statistics_tracking();
    run_test_print_stats();

    /* T101: Abnormal tests */
    run_test_invalid_core_check();

    /* T201-T202: Boundary tests */
    run_test_single_core_boundary();
    run_test_max_cores_boundary();

    /* T010: Migration test */
    run_test_migration_basic();

    printf("\n=== Test Summary ===\n");
    printf("Total tests run: %d\n", tests_run);
    printf("Passed: %d\n", tests_passed);
    printf("Failed: %d\n", tests_failed);

    if (tests_failed == 0) {
        printf("\nAll tests PASSED!\n");
        return 0;
    } else {
        printf("\nSome tests FAILED!\n");
        return 1;
    }
}

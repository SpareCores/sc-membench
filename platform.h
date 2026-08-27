/*
 * Copyright 2026 Spare Cores
 * Licensed under Mozilla Public License 2.0
 */

#ifndef MEMBENCH_PLATFORM_H
#define MEMBENCH_PLATFORM_H

/* Platform detection (may be overridden by compiler flags) */
#if !defined(PLATFORM_LINUX) && !defined(PLATFORM_MACOS) && !defined(PLATFORM_BSD)
    #if defined(__linux__)
        #define PLATFORM_LINUX
    #elif defined(__APPLE__) && defined(__MACH__)
        #define PLATFORM_MACOS
    #elif defined(__FreeBSD__) || defined(__OpenBSD__) || defined(__NetBSD__) || defined(__DragonFly__)
        #define PLATFORM_BSD
    #endif
#endif

#include <stddef.h>

#include <stdio.h>
#include <stdlib.h>

/* Platform-specific includes */
#ifdef PLATFORM_LINUX
#include <sched.h>
#endif

#ifdef PLATFORM_BSD
#include <sys/param.h>
#include <sys/cpuset.h>
#include <sys/sysctl.h>
#endif

#ifdef PLATFORM_MACOS
#include <sys/sysctl.h>
#include <mach/mach.h>
#include <mach/thread_policy.h>
#endif

/* Optional library: libhugetlbfs (Linux only, for huge page size detection) */
#if defined(HAVE_HUGETLBFS) && defined(PLATFORM_LINUX)
#include <hugetlbfs.h>
#endif

/* Optional library: NUMA support (Linux only) */
#ifdef USE_NUMA
#include <numa.h>
#include <numaif.h>
#endif


typedef struct {
    int num_cpus;
    size_t total_memory;
    size_t l1_cache_size;     /* per core */
    size_t l2_cache_size;
    size_t l3_cache_size;
    int numa_nodes;
} platform_info_t;


/* verbose: 0=quiet, 1=summary, 2=detailed (stderr) */
void platform_init(platform_info_t *pi, int verbose);
void platform_deinit(void);

/* ============================================================================
 * Huge pages
 * ============================================================================ */

/* Get huge page size dynamically from the system.
 * Tries multiple methods in order of reliability:
 *   1. libhugetlbfs (if available, most reliable)
 *   2. /proc/meminfo (Linux)
 *   3. sysctl (macOS/BSD)
 *   4. Default fallback (2MB for x86, common size)
 * Returns the default huge page size (typically 2MB on x86, varies on ARM). */
static inline size_t get_huge_page_size(void) {
    static size_t cached_size = 0;
    if (cached_size != 0) return cached_size;

#if defined(HAVE_HUGETLBFS) && defined(PLATFORM_LINUX)
    /* Method 1: libhugetlbfs (most reliable on Linux) */
    long size = gethugepagesize();
    if (size > 0) {
        cached_size = (size_t)size;
        return cached_size;
    }
#endif

#ifdef PLATFORM_LINUX
    /* Method 2: Parse /proc/meminfo */
    FILE *file = fopen("/proc/meminfo", "r");
    if (file) {
        char line[256];
        unsigned long size_kb = 0;
        while (fgets(line, sizeof(line), file)) {
            if (sscanf(line, "Hugepagesize: %lu kB", &size_kb) == 1) {
                cached_size = size_kb * 1024;
                fclose(file);
                return cached_size;
            }
        }
        fclose(file);
    }
#endif

#if defined(PLATFORM_MACOS) || defined(PLATFORM_BSD)
    /* Method 3: sysctl for macOS/BSD (get VM page size, huge pages vary) */
    /* Note: macOS doesn't have traditional huge pages like Linux,
     * but we can use vm.pagesize as a reference. Superpage support varies. */
    int mib[2] = { CTL_HW, HW_PAGESIZE };
    int pagesize = 0;
    size_t len = sizeof(pagesize);
    if (sysctl(mib, 2, &pagesize, &len, NULL, 0) == 0 && pagesize > 0) {
        /* On macOS, superpage size is typically 2MB on Intel, 16KB on ARM
         * but there's no standard API to query it. Use 2MB as common default. */
        cached_size = 2UL * 1024 * 1024;
        return cached_size;
    }
#endif

    /* Method 4: Default fallback (2MB, most common huge page size) */
    cached_size = 2UL * 1024 * 1024;
    return cached_size;
}

/* Minimum buffer size to use huge pages (2 huge pages).
 * Below this threshold, TLB pressure isn't significant and huge pages
 * would waste memory (each allocation rounds up to huge page boundary). */
static inline size_t get_huge_page_threshold(void) {
    return 2 * get_huge_page_size();
}

/* ============================================================================
 * CPU pinning
 * ============================================================================ */

/* Pin current thread to CPU 0 for consistent latency measurement.
 * Platform-specific implementations for Linux, macOS, and BSD. */
static inline void pin_thread_to_cpu0(int verbose) {
    int success = 0;

#ifdef PLATFORM_LINUX
    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    success = (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) == 0);
#endif

#ifdef PLATFORM_BSD
    cpuset_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(0, &cpuset);
    success = (cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1,
                                   sizeof(cpuset), &cpuset) == 0);
#endif

#ifdef PLATFORM_MACOS
    /* macOS doesn't have true CPU affinity, but we can suggest affinity
     * via thread_policy_set with THREAD_AFFINITY_POLICY.
     * This is a hint, not a guarantee. */
    thread_affinity_policy_data_t policy = { 0 };  /* Affinity tag 0 */
    success = (thread_policy_set(mach_thread_self(), THREAD_AFFINITY_POLICY,
                                 (thread_policy_t)&policy,
                                 THREAD_AFFINITY_POLICY_COUNT) == KERN_SUCCESS);
#endif

    if (verbose >= 2) {
        if (success) {
            fprintf(stderr, "  Latency thread pinned to CPU 0\n");
        } else {
            fprintf(stderr, "  Warning: Could not pin thread to CPU 0\n");
        }
    }
    (void)success;  /* Suppress unused warning if no platform matched */
}

#endif /* MEMBENCH_PLATFORM_H */

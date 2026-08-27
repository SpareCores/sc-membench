/*
 * Copyright 2026 Spare Cores
 * Licensed under Mozilla Public License 2.0
 */

#ifndef MEMBENCH_RUNNER_H
#define MEMBENCH_RUNNER_H

#include <stdio.h>
#include <stdlib.h>

#include "config.h"
#include "membench.h"
#include "bandwidth.h"
#include "latency.h"
#include "platform.h"
#include "report.h"
#include "utils.h"

static volatile int g_running = 1;

/* ============================================================================
 * Main benchmark loop
 * ============================================================================ */

/* Generate thread counts dynamically based on CPU count (for auto-scaling mode)
 *
 * Strategy:
 * - Powers of 2 from 1 up to nproc
 * - Always include nproc itself (if not already a power of 2)
 * - No oversubscription (causes unreliable results)
 *
 * Examples:
 *   4 cores:   1, 2, 4               (3 values)
 *   32 cores:  1, 2, 4, 8, 16, 32    (6 values)
 *   48 cores:  1, 2, 4, 8, 16, 32, 48 (7 values)
 */
static int* get_thread_counts(const platform_info_t *pi, int *count) {
    int nproc = pi->num_cpus;
    if (nproc < 1) nproc = 1;

    /* Cap at nproc - oversubscription causes unreliable benchmark results
     * due to context switching, cache thrashing, and scheduler interference */
    int max_threads = nproc;

    /* Allocate more than enough space */
    int *tc = malloc(32 * sizeof(int));
    int n = 0;

    /* Add powers of 2 up to nproc */
    for (int t = 1; t <= max_threads; t *= 2) {
        tc[n++] = t;
    }

    /* Add nproc if not already in list (i.e., not a power of 2) */
    if (tc[n-1] != nproc) {
        tc[n++] = nproc;
    }

    tc[n] = 0;  /* Sentinel */
    *count = n;
    return tc;
}

/* Get sizes to test (per-thread buffer sizes) - adaptive based on cache hierarchy
 *
 * Generates sizes at critical cache transition points to show:
 * 1. Pure L1 performance
 * 2. L1→L2 transition
 * 3. Pure L2 performance
 * 4. L2→L3 transition
 * 5. L3 region
 * 6. Pure RAM bandwidth
 *
 * All sizes are strictly increasing with no overlaps.
 */
static size_t* get_sizes(const platform_info_t *pi, const bench_config_t *cfg, int *count) {
    int nthreads = cfg->explicit_threads > 0 ? cfg->explicit_threads : pi->num_cpus;
    if (nthreads < 1) nthreads = 1;

    /* Use detected cache sizes, with sensible defaults */
    size_t l1 = pi->l1_cache_size > 0 ? pi->l1_cache_size : 32768;      /* 32 KB */
    size_t l2 = pi->l2_cache_size > 0 ? pi->l2_cache_size : 262144;     /* 256 KB */
    size_t l3 = pi->l3_cache_size > 0 ? pi->l3_cache_size : 8388608;    /* 8 MB */

    /* Memory limit per thread */
    size_t max_size = pi->total_memory / 2 / nthreads;

    /* Build strictly increasing size sequence */
    size_t sizes_list[20];
    int n = 0;
    size_t prev = 0;

    /* Helper macro to add size if > prev and <= max_size */
    #define ADD_SIZE(sz) do { \
        size_t _s = round_to_power_of_2(sz); \
        if (_s > prev && _s <= max_size) { sizes_list[n++] = _s; prev = _s; } \
    } while(0)

    /* L1 region */
    ADD_SIZE(l1 / 2);

    /* L1→L2 transition */
    ADD_SIZE(l1 * 2);

    /* L2 region */
    ADD_SIZE(l2 / 2);
    ADD_SIZE(l2);

    /* L2→L3 transition */
    ADD_SIZE(l2 * 2);

    /* L3 region */
    if (l3 > l2 * 4) {
        ADD_SIZE(l3 / 4);
    }
    ADD_SIZE(l3 / 2);

    /* L3→RAM transition */
    ADD_SIZE(l3);

    /* RAM region */
    ADD_SIZE(l3 * 2);
    ADD_SIZE(l3 * 4);

    /* Full sweep: add larger sizes up to memory limit */
    if (cfg->full_sweep) {
        size_t ram_size = RAM_SIZE_2 * 2;
        while (ram_size <= max_size && n < 18) {
            ADD_SIZE(ram_size);
            ram_size *= 2;
        }
    }

    #undef ADD_SIZE

    /* Ensure at least one size */
    if (n == 0) {
        sizes_list[n++] = 4096;
    }

    /* Copy to result array */
    size_t *sizes = malloc((n + 1) * sizeof(size_t));
    for (int i = 0; i < n; i++) {
        sizes[i] = sizes_list[i];
    }
    sizes[n] = 0;
    *count = n;
    return sizes;
}

/* Find best configuration for a given buffer size and operation.
 *
 * This follows bw_mem's approach:
 * - buffer_size is the per-thread buffer size
 * - Total memory = buffer_size * threads (or buffer_size * threads * 2 for copy)
 *
 * Three modes:
 * 1. Auto-scaling (cfg->auto_scaling=1): Try multiple thread counts, find best
 * 2. Explicit threads (cfg->explicit_threads>0): Use exactly that many threads
 * 3. Default (neither): Use num_cpus threads
 */
static result_t find_best_config(const platform_info_t *pi, const bench_config_t *cfg, size_t buffer_size, operation_t op,
                                 int *thread_counts, int tc_count) {
    result_t best = {0};
    best.size = buffer_size;
    best.op = op;

    /* For latency test: single-thread, statistically valid measurement */
    if (op == OP_LATENCY) {
        size_t max_latency = MAX_LATENCY_SIZE;
        if (pi->total_memory / 4 < max_latency) {
            max_latency = pi->total_memory / 4;
        }
        size_t latency_size = (buffer_size > max_latency) ? max_latency : buffer_size;

        double start = get_time();
        latency_stats_t stats = measure_latency_stats(pi, cfg, latency_size);
        double elapsed = get_time() - start;

        best.size = buffer_size;
        best.op = op;
        best.threads = 1;
        best.latency_ns = stats.median_ns;
        best.latency_mean_ns = stats.mean_ns;
        best.latency_stddev_ns = stats.stddev_ns;
        best.latency_cv = stats.cv;
        best.latency_samples = stats.num_samples;
        best.elapsed_s = elapsed;
        best.iterations = stats.num_samples;

        return best;
    }

    /* Bandwidth tests */
    int nthreads;

    if (cfg->auto_scaling) {
        /* Auto-scaling mode: try all thread counts, find best */
        for (int i = 0; i < tc_count; i++) {
            nthreads = thread_counts[i];
            if (nthreads < 1) continue;

            int bufs_per_op = (op == OP_COPY) ? 2 : 1;
            size_t memory_needed = buffer_size * nthreads * bufs_per_op;
            if (memory_needed > pi->total_memory / 4) {
                continue;
            }

            result_t r = run_benchmark_best(cfg, buffer_size, op, nthreads);
            r.size = buffer_size;

            if (r.bandwidth_mb_s > best.bandwidth_mb_s) {
                best = r;
            }
        }

        if (best.bandwidth_mb_s == 0) {
            best = run_benchmark_best(cfg, buffer_size, op, 1);
            best.size = buffer_size;
        }

        return best;
    }

    /* Fixed thread count mode */
    if (cfg->explicit_threads > 0) {
        nthreads = cfg->explicit_threads;
    } else {
        nthreads = pi->num_cpus;
    }

    /* Check memory limit and reduce threads if needed */
    int bufs_per_op = (op == OP_COPY) ? 2 : 1;
    size_t memory_needed = buffer_size * nthreads * bufs_per_op;
    while (nthreads > 1 && memory_needed > pi->total_memory / 4) {
        nthreads /= 2;
        memory_needed = buffer_size * nthreads * bufs_per_op;
    }

    best = run_benchmark_best(cfg, buffer_size, op, nthreads);
    best.size = buffer_size;

    return best;
}

static void run_all_benchmarks(const platform_info_t *pi, const bench_config_t *cfg) {
    double start_time = get_time();

    int tc_count;
    int *thread_counts = get_thread_counts(pi, &tc_count);

    /* Single size mode */
    if (cfg->single_size > 0) {
        if (cfg->verbose) {
            fprintf(stderr, "Testing buffer size: %zu KB per thread\n",
                    cfg->single_size / 1024);
        }

        print_csv_header(cfg);

        for (int op = 0; op < 4 && g_running; op++) {
            if (!(cfg->ops_mask & (1 << op))) continue;

            result_t best = find_best_config(pi, cfg, cfg->single_size, (operation_t)op,
                                            thread_counts, tc_count);

            if (best.bandwidth_mb_s > 0 || best.latency_ns > 0) {
                print_result(cfg, &best);
                if (cfg->human_readable) update_summary(&best);
                fflush(stdout);
            }
        }

        free(thread_counts);

        if (cfg->verbose) {
            double total = get_time() - start_time;
            fprintf(stderr, "Total runtime: %.1f seconds\n", total);
        }

        /* Print summary in human-readable mode */
        if (cfg->human_readable) print_summary(pi, cfg);
        return;
    }

    /* Normal mode: test all sizes */
    int size_count;
    size_t *sizes = get_sizes(pi, cfg, &size_count);

    if (cfg->verbose) {
        fprintf(stderr, "Testing %d buffer sizes (per thread, adaptive to cache hierarchy)\n", size_count);
        if (cfg->auto_scaling) {
            fprintf(stderr, "Thread mode: auto-scaling (trying 1-%d threads)\n", pi->num_cpus);
        } else if (cfg->explicit_threads > 0) {
            fprintf(stderr, "Thread mode: fixed %d threads\n", cfg->explicit_threads);
        } else {
            fprintf(stderr, "Thread mode: num_cpus (%d threads)\n", pi->num_cpus);
        }
        fprintf(stderr, "OpenMP: proc_bind(spread) for NUMA-aware thread placement\n");
    }

    print_csv_header(cfg);

    for (int s = 0; s < size_count && g_running; s++) {
        size_t size = sizes[s];

        for (int op = 0; op < 4 && g_running; op++) {
            if (!(cfg->ops_mask & (1 << op))) continue;

            result_t best = find_best_config(pi, cfg, size, (operation_t)op,
                                             thread_counts, tc_count);

            if (best.bandwidth_mb_s > 0 || best.latency_ns > 0) {
                print_result(cfg, &best);
                if (cfg->human_readable) update_summary(&best);
                fflush(stdout);
            }

            if (cfg->max_runtime > 0) {
                double elapsed = get_time() - start_time;
                if (elapsed > cfg->max_runtime) {
                    if (cfg->verbose) {
                        fprintf(stderr, "Time limit reached (%.1f s)\n", elapsed);
                    }
                    g_running = 0;
                    break;
                }
            }
        }
    }

    free(sizes);
    free(thread_counts);

    if (cfg->verbose) {
        double total = get_time() - start_time;
        fprintf(stderr, "Total runtime: %.1f seconds\n", total);
    }

    /* Print summary in human-readable mode */
    if (cfg->human_readable) print_summary(pi, cfg);
}

#endif /* MEMBENCH_RUNNER_H */

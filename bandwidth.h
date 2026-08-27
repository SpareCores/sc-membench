/*
 * Copyright 2026 Spare Cores
 * Licensed under Mozilla Public License 2.0
 */

#ifndef MEMBENCH_BANDWIDTH_H
#define MEMBENCH_BANDWIDTH_H

#include <omp.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

#include "config.h"
#include "membench.h"
#include "platform.h"
#include "utils.h"

/* Optional library: NUMA support (Linux only) */
#ifdef USE_NUMA
#include <numa.h>
#include <numaif.h>
#endif

/* ============================================================================
 * Memory operations
 * ============================================================================ */

/* Prevent compiler from optimizing away operations */
static volatile uint64_t g_sink = 0;

/*
 * Memory operations - heavily optimized for bandwidth measurement
 * Key techniques:
 * 1. Multiple independent accumulators to break dependency chains
 * 2. Large unrolling (32 elements = 256 bytes per iteration)
 * 3. Force inlining to eliminate call overhead
 */

/* Read operation: XOR all 64-bit words with independent accumulators
 * XOR is faster than ADD and has no carry dependency chains */
static inline __attribute__((always_inline))
uint64_t mem_read(const void *buf, size_t size) {
    const uint64_t *p = (const uint64_t *)buf;
    const uint64_t *end = p + (size / sizeof(uint64_t));

    /* Use 8 independent accumulators - each one handles every 8th element */
    uint64_t x0 = 0, x1 = 0, x2 = 0, x3 = 0;
    uint64_t x4 = 0, x5 = 0, x6 = 0, x7 = 0;

    /* Process 32 elements (256 bytes) per iteration */
    while (p + 32 <= end) {
        x0 ^= p[0];  x1 ^= p[1];  x2 ^= p[2];  x3 ^= p[3];
        x4 ^= p[4];  x5 ^= p[5];  x6 ^= p[6];  x7 ^= p[7];
        x0 ^= p[8];  x1 ^= p[9];  x2 ^= p[10]; x3 ^= p[11];
        x4 ^= p[12]; x5 ^= p[13]; x6 ^= p[14]; x7 ^= p[15];
        x0 ^= p[16]; x1 ^= p[17]; x2 ^= p[18]; x3 ^= p[19];
        x4 ^= p[20]; x5 ^= p[21]; x6 ^= p[22]; x7 ^= p[23];
        x0 ^= p[24]; x1 ^= p[25]; x2 ^= p[26]; x3 ^= p[27];
        x4 ^= p[28]; x5 ^= p[29]; x6 ^= p[30]; x7 ^= p[31];
        p += 32;
    }

    /* Handle remaining elements */
    while (p + 8 <= end) {
        x0 ^= p[0]; x1 ^= p[1]; x2 ^= p[2]; x3 ^= p[3];
        x4 ^= p[4]; x5 ^= p[5]; x6 ^= p[6]; x7 ^= p[7];
        p += 8;
    }
    while (p < end) {
        x0 ^= *p++;
    }

    return x0 ^ x1 ^ x2 ^ x3 ^ x4 ^ x5 ^ x6 ^ x7;
}

/* Write operation: fill with pattern, heavily unrolled */
static inline __attribute__((always_inline))
void mem_write(void *buf, size_t size, uint64_t pattern) {
    uint64_t *p = (uint64_t *)buf;
    uint64_t *end = p + (size / sizeof(uint64_t));

    /* Process 32 elements (256 bytes) per iteration */
    while (p + 32 <= end) {
        p[0]  = pattern; p[1]  = pattern; p[2]  = pattern; p[3]  = pattern;
        p[4]  = pattern; p[5]  = pattern; p[6]  = pattern; p[7]  = pattern;
        p[8]  = pattern; p[9]  = pattern; p[10] = pattern; p[11] = pattern;
        p[12] = pattern; p[13] = pattern; p[14] = pattern; p[15] = pattern;
        p[16] = pattern; p[17] = pattern; p[18] = pattern; p[19] = pattern;
        p[20] = pattern; p[21] = pattern; p[22] = pattern; p[23] = pattern;
        p[24] = pattern; p[25] = pattern; p[26] = pattern; p[27] = pattern;
        p[28] = pattern; p[29] = pattern; p[30] = pattern; p[31] = pattern;
        p += 32;
    }

    /* Handle remaining */
    while (p < end) {
        *p++ = pattern;
    }
}

/* Copy operation: copy from src to dst, heavily unrolled */
static inline __attribute__((always_inline))
void mem_copy(void *dst, const void *src, size_t size) {
    const uint64_t *s = (const uint64_t *)src;
    uint64_t *d = (uint64_t *)dst;
    const uint64_t *end = s + (size / sizeof(uint64_t));

    /* Process 32 elements (256 bytes) per iteration */
    while (s + 32 <= end) {
        d[0]  = s[0];  d[1]  = s[1];  d[2]  = s[2];  d[3]  = s[3];
        d[4]  = s[4];  d[5]  = s[5];  d[6]  = s[6];  d[7]  = s[7];
        d[8]  = s[8];  d[9]  = s[9];  d[10] = s[10]; d[11] = s[11];
        d[12] = s[12]; d[13] = s[13]; d[14] = s[14]; d[15] = s[15];
        d[16] = s[16]; d[17] = s[17]; d[18] = s[18]; d[19] = s[19];
        d[20] = s[20]; d[21] = s[21]; d[22] = s[22]; d[23] = s[23];
        d[24] = s[24]; d[25] = s[25]; d[26] = s[26]; d[27] = s[27];
        d[28] = s[28]; d[29] = s[29]; d[30] = s[30]; d[31] = s[31];
        s += 32;
        d += 32;
    }

    /* Handle remaining */
    while (s < end) {
        *d++ = *s++;
    }
}

/* ============================================================================
 * Memory allocation
 * ============================================================================ */

static void* alloc_buffer(const bench_config_t *cfg, size_t size) {
    void *buf = MAP_FAILED;
    int try_hugepages = cfg->use_hugepages && (size >= get_huge_page_threshold());

    if (try_hugepages) {
        /*
         * Strategy: prefer THP over explicit huge pages because:
         * 1. THP doesn't require pre-allocation by root
         * 2. THP is managed automatically by the kernel
         * 3. Explicit huge pages may fail if pool isn't configured
         *
         * We try explicit huge pages first only because they're more
         * deterministic (guaranteed 2MB pages vs THP's best-effort).
         */

#ifdef MAP_HUGETLB
        /* Round up size to huge page boundary for explicit huge pages */
        size_t hp_size = get_huge_page_size();
        size_t aligned_size = (size + hp_size - 1) & ~(hp_size - 1);

        /* Try explicit huge pages (uses pre-allocated pool if available) */
        buf = mmap(NULL, aligned_size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
        if (buf != MAP_FAILED) {
            if (cfg->verbose >= 2) {
                fprintf(stderr, "  Allocated %zu bytes using explicit %zu KB huge pages\n",
                        aligned_size, hp_size / 1024);
            }
            /* Touch all pages to ensure they're allocated */
            memset(buf, 0, size);
            return buf;
        }
        /* Explicit huge pages failed - likely no pool configured, try THP */
#endif

        /* Use mmap + madvise for Transparent Huge Pages (no pre-allocation needed) */
        buf = mmap(NULL, size, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        if (buf != MAP_FAILED) {
#ifdef MADV_HUGEPAGE
            /* Hint to kernel: please use huge pages for this region.
             * The kernel will use THP if available and beneficial.
             * This doesn't require root or pre-allocation. */
            if (madvise(buf, size, MADV_HUGEPAGE) == 0) {
                if (cfg->verbose >= 2) {
                    fprintf(stderr, "  Allocated %zu bytes with THP (transparent huge pages)\n", size);
                }
            } else if (cfg->verbose >= 2) {
                fprintf(stderr, "  Allocated %zu bytes (THP hint failed, using regular pages)\n", size);
            }
#else
            if (cfg->verbose >= 2) {
                fprintf(stderr, "  Allocated %zu bytes (THP not available on this system)\n", size);
            }
#endif
            /* Touch all pages to ensure they're allocated */
            memset(buf, 0, size);
            return buf;
        }
    }

    /* Regular allocation: small buffers, huge pages disabled, or fallback */
    buf = mmap(NULL, size, PROT_READ | PROT_WRITE,
               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);

    if (buf == MAP_FAILED) {
        return NULL;
    }

    /* Touch all pages to ensure they're allocated */
    memset(buf, 0, size);

    return buf;
}

static void free_buffer(void *buf, size_t size) {
    if (buf) {
        munmap(buf, size);
    }
}

/* ============================================================================
 * OpenMP Bandwidth Benchmark
 * ============================================================================ */

/*
 * Run bandwidth benchmark using OpenMP.
 *
 * Key features:
 * - proc_bind(spread) distributes threads across NUMA nodes
 * - Per-thread NUMA-local buffer allocation
 * - Implicit barrier synchronization (more efficient than pthread_barrier)
 * - 8-accumulator read for optimal bandwidth measurement
 */
static result_t run_benchmark_omp(const bench_config_t *cfg, size_t size, operation_t op, int nthreads) {
    result_t result = {0};
    result.size = size;
    result.op = op;
    result.threads = nthreads;

    /* Allocate arrays for per-thread buffers and results */
    void **src_bufs = calloc(nthreads, sizeof(void*));
    void **dst_bufs = calloc(nthreads, sizeof(void*));
    double *thread_elapsed = calloc(nthreads, sizeof(double));
    uint64_t *thread_checksums = calloc(nthreads, sizeof(uint64_t));
    int alloc_failed = 0;

    if (!src_bufs || !dst_bufs || !thread_elapsed || !thread_checksums) {
        free(src_bufs);
        free(dst_bufs);
        free(thread_elapsed);
        free(thread_checksums);
        return result;
    }

    /* Set OpenMP thread count */
    omp_set_num_threads(nthreads);

    /* Phase 1: Parallel allocation with NUMA awareness
     * proc_bind(spread) distributes threads across NUMA nodes,
     * then each thread allocates memory locally */
    #pragma omp parallel proc_bind(spread)
    {
        int tid = omp_get_thread_num();

#ifdef USE_NUMA
        /* Get current CPU and its NUMA node (OpenMP has placed us optimally) */
        if (numa_available() >= 0) {
            int cpu = sched_getcpu();
            int node = numa_node_of_cpu(cpu);
            if (node >= 0) {
                /* Allocate on local NUMA node */
                src_bufs[tid] = numa_alloc_onnode(size, node);
                if (op == OP_COPY) {
                    dst_bufs[tid] = numa_alloc_onnode(size, node);
                }
            }
        }
#endif

        /* Fallback: regular allocation if NUMA not available or failed */
        if (!src_bufs[tid]) {
            src_bufs[tid] = alloc_buffer(cfg, size);
        }
        if (op == OP_COPY && !dst_bufs[tid]) {
            dst_bufs[tid] = alloc_buffer(cfg, size);
        }

        /* Check allocation success */
        if (!src_bufs[tid] || (op == OP_COPY && !dst_bufs[tid])) {
            #pragma omp atomic write
            alloc_failed = 1;
        }

        /* Initialize buffer (first-touch for NUMA) */
        if (src_bufs[tid]) {
            memset(src_bufs[tid], 0xAA, size);
        }
        if (dst_bufs[tid]) {
            memset(dst_bufs[tid], 0, size);
        }
    }

    if (alloc_failed) {
        /* Cleanup on allocation failure */
        for (int i = 0; i < nthreads; i++) {
#ifdef USE_NUMA
            if (numa_available() >= 0) {
                if (src_bufs[i]) numa_free(src_bufs[i], size);
                if (dst_bufs[i]) numa_free(dst_bufs[i], size);
            } else
#endif
            {
                free_buffer(src_bufs[i], size);
                free_buffer(dst_bufs[i], size);
            }
        }
        free(src_bufs);
        free(dst_bufs);
        free(thread_elapsed);
        free(thread_checksums);
        if (cfg->verbose) {
            fprintf(stderr, "Failed to allocate %zu bytes × %d threads\n", size, nthreads);
        }
        return result;
    }

    /* Phase 2: Calibration - estimate iterations needed */
    int iterations = MIN_ITERATIONS;
    {
        /* Warmup */
        g_sink += mem_read(src_bufs[0], size);

        /* Time single iteration */
        double t_start = get_time();
        switch (op) {
            case OP_READ:
                g_sink += mem_read(src_bufs[0], size);
                break;
            case OP_WRITE:
                mem_write(src_bufs[0], size, 0x1234567890ABCDEFULL);
                break;
            case OP_COPY:
                mem_copy(dst_bufs[0], src_bufs[0], size);
                break;
            default:
                break;
        }
        double time_per_iter = get_time() - t_start;

        if (time_per_iter > 1e-9) {
            iterations = (int)(TARGET_TIME_PER_TEST / time_per_iter);
            if (iterations < MIN_ITERATIONS) iterations = MIN_ITERATIONS;
            if (iterations > MAX_ITERATIONS) iterations = MAX_ITERATIONS;
        }
    }
    result.iterations = iterations;

    /* Phase 3: Timed measurement with all threads
     * OpenMP implicit barrier ensures all threads start together */
    #pragma omp parallel proc_bind(spread)
    {
        int tid = omp_get_thread_num();
        void *src = src_bufs[tid];
        void *dst = dst_bufs[tid];
        uint64_t checksum = 0;

        /* Implicit barrier here - all threads synchronized */

        double t_start = get_time();

        switch (op) {
            case OP_READ:
                for (int i = 0; i < iterations; i++) {
                    checksum ^= mem_read(src, size);
                }
                break;
            case OP_WRITE:
                for (int i = 0; i < iterations; i++) {
                    mem_write(src, size, (uint64_t)i);
                }
                break;
            case OP_COPY:
                for (int i = 0; i < iterations; i++) {
                    mem_copy(dst, src, size);
                }
                break;
            default:
                break;
        }

        double t_end = get_time();

        thread_elapsed[tid] = t_end - t_start;
        thread_checksums[tid] = checksum;
    }

    /* Find max elapsed time (determines overall bandwidth) */
    double max_elapsed = 0;
    uint64_t total_checksum = 0;
    for (int i = 0; i < nthreads; i++) {
        if (thread_elapsed[i] > max_elapsed) {
            max_elapsed = thread_elapsed[i];
        }
        total_checksum ^= thread_checksums[i];
    }

    g_sink += total_checksum;
    result.elapsed_s = max_elapsed;

    /* Calculate bandwidth = (size per thread * threads * iterations) / time
     * This gives aggregate bandwidth across all threads.
     * Note: for copy, we report buffer size (not 2x) to match bw_mem convention */
    if (max_elapsed > 0) {
        size_t bytes_transferred = (size_t)size * nthreads * iterations;
        result.bandwidth_mb_s = (bytes_transferred / (1024.0 * 1024.0)) / max_elapsed;
    }

    /* Cleanup */
    for (int i = 0; i < nthreads; i++) {
#ifdef USE_NUMA
        if (numa_available() >= 0) {
            if (src_bufs[i]) numa_free(src_bufs[i], size);
            if (dst_bufs[i]) numa_free(dst_bufs[i], size);
        } else
#endif
        {
            free_buffer(src_bufs[i], size);
            free_buffer(dst_bufs[i], size);
        }
    }
    free(src_bufs);
    free(dst_bufs);
    free(thread_elapsed);
    free(thread_checksums);

    return result;
}

/* Run single-threaded benchmark (for small buffers) */
static result_t run_benchmark_single(const bench_config_t *cfg, size_t size, operation_t op) {
    result_t result = {0};
    result.size = size;
    result.op = op;
    result.threads = 1;

    void *src = alloc_buffer(cfg, size);
    void *dst = (op == OP_COPY) ? alloc_buffer(cfg, size) : NULL;

    if (!src || (op == OP_COPY && !dst)) {
        free_buffer(src, size);
        free_buffer(dst, size);
        return result;
    }

    memset(src, 0xAA, size);
    if (dst) memset(dst, 0, size);

    /* Warmup */
    g_sink += mem_read(src, size);

    /* Calibrate */
    double t_start = get_time();
    switch (op) {
        case OP_READ: g_sink += mem_read(src, size); break;
        case OP_WRITE: mem_write(src, size, 0x1234567890ABCDEFULL); break;
        case OP_COPY: mem_copy(dst, src, size); break;
        default: break;
    }
    double time_per_iter = get_time() - t_start;

    int iterations = MIN_ITERATIONS;
    if (time_per_iter > 1e-9) {
        iterations = (int)(TARGET_TIME_PER_TEST / time_per_iter);
        if (iterations < MIN_ITERATIONS) iterations = MIN_ITERATIONS;
        if (iterations > MAX_ITERATIONS) iterations = MAX_ITERATIONS;
    }
    result.iterations = iterations;

    /* Timed run */
    uint64_t checksum = 0;
    t_start = get_time();

    switch (op) {
        case OP_READ:
            for (int i = 0; i < iterations; i++) {
                checksum ^= mem_read(src, size);
            }
            break;
        case OP_WRITE:
            for (int i = 0; i < iterations; i++) {
                mem_write(src, size, (uint64_t)i);
            }
            break;
        case OP_COPY:
            for (int i = 0; i < iterations; i++) {
                mem_copy(dst, src, size);
            }
            break;
        default:
            break;
    }

    double elapsed = get_time() - t_start;

    g_sink += checksum;
    result.elapsed_s = elapsed;

    if (elapsed > 0) {
        size_t bytes_transferred = size * iterations;
        result.bandwidth_mb_s = (bytes_transferred / (1024.0 * 1024.0)) / elapsed;
    }

    free_buffer(src, size);
    free_buffer(dst, size);

    return result;
}

/* Main benchmark runner - dispatches to OpenMP or single-threaded */
static result_t run_benchmark(const bench_config_t *cfg, size_t size, operation_t op, int nthreads) {
    if (nthreads == 1) {
        return run_benchmark_single(cfg, size, op);
    }
    return run_benchmark_omp(cfg, size, op, nthreads);
}

/* Run benchmark multiple times and return the best (highest bandwidth) result,
 * like lmbench TRIES.
 *
 * First run is a warmup (discarded) to allow CPU frequency to ramp up
 * and caches to warm. This dramatically reduces result variability.
 */
static result_t run_benchmark_best(const bench_config_t *cfg, size_t size, operation_t op, int nthreads) {
    result_t best = {0};

    /* Warmup run - discarded.
     * This allows: CPU to reach turbo frequency, caches to warm,
     * thread scheduling to stabilize. Critical for consistent results. */
    (void)run_benchmark(cfg, size, op, nthreads);

    for (int try = 0; try < cfg->benchmark_tries; try++) {
        result_t r = run_benchmark(cfg, size, op, nthreads);

        if (try == 0 || r.bandwidth_mb_s > best.bandwidth_mb_s) {
            best = r;
        }
    }

    return best;
}

#endif /* MEMBENCH_BANDWIDTH_H */

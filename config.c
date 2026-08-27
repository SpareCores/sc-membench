/*
 * Copyright 2026 Spare Cores
 * Licensed under Mozilla Public License 2.0
 */

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "membench.h"

static void usage(const char *prog) {
    fprintf(stderr, "sc-membench %s - Memory Bandwidth Benchmark (OpenMP)\n\n", VERSION);
    fprintf(stderr, "Usage: %s [options]\n\n", prog);
    fprintf(stderr, "Options:\n");
    fprintf(stderr, "  -h          Show this help\n");
    fprintf(stderr, "  -V          Print version and exit\n");
    fprintf(stderr, "  -v          Verbose output (use -vv for more detail)\n");
    fprintf(stderr, "  -s SIZE_KB  Test only this buffer size (in KB), e.g. -s 1024 for 1MB\n");
    fprintf(stderr, "  -f          Full sweep (test all sizes up to memory limit)\n");
    fprintf(stderr, "              Default: test up to 512 MB per thread\n");
    fprintf(stderr, "  -p THREADS  Use exactly this many threads (default: num_cpus)\n");
    fprintf(stderr, "  -a          Auto-scaling: try different thread counts to find best\n");
    fprintf(stderr, "              (slower but finds optimal thread count per buffer size)\n");
    fprintf(stderr, "  -t SECONDS  Maximum runtime, 0 = unlimited (default: unlimited)\n");
    fprintf(stderr, "  -r TRIES    Repeat each test N times, report best (default: %d)\n", DEFAULT_BENCHMARK_TRIES);
    fprintf(stderr, "  -o OP       Run only this operation: read, write, copy, or latency\n");
    fprintf(stderr, "              Can be specified multiple times (default: all)\n");
    fprintf(stderr, "  -H          Enable huge pages for large buffers (>= 4MB)\n");
    fprintf(stderr, "              Uses THP (no setup needed) or explicit 2MB pages\n");
    fprintf(stderr, "              Automatically skipped for small buffers\n");
    fprintf(stderr, "  -R          Human-readable output with summary (default: CSV)\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "OpenMP Thread Affinity (environment variables):\n");
    fprintf(stderr, "  OMP_PROC_BIND=spread  Spread threads across NUMA nodes (default)\n");
    fprintf(stderr, "  OMP_PLACES=cores      One thread per physical core\n");
    fprintf(stderr, "  OMP_NUM_THREADS=N     Override thread count\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Output: CSV to stdout with columns:\n");
    fprintf(stderr, "  size_kb           - Per-thread buffer size (KB)\n");
    fprintf(stderr, "  operation         - read, write, copy, or latency\n");
    fprintf(stderr, "  bandwidth_mb_s    - Aggregate bandwidth in MB/s (0 for latency)\n");
    fprintf(stderr, "  latency_ns        - Median memory latency in ns (0 for bandwidth)\n");
    fprintf(stderr, "  latency_stddev_ns - Latency standard deviation in ns (0 for bandwidth)\n");
    fprintf(stderr, "  latency_samples   - Number of samples for latency measurement\n");
    fprintf(stderr, "  threads           - Thread count used\n");
    fprintf(stderr, "  iterations        - Iterations performed\n");
    fprintf(stderr, "  elapsed_s         - Elapsed time in seconds\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Latency measurement uses linked list traversal with random node order\n");
    fprintf(stderr, "to defeat prefetchers. Statistical validity ensured via multiple samples\n");
    fprintf(stderr, "until coefficient of variation < 5%% or max samples reached.\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Memory model: each thread gets its own buffer.\n");
    fprintf(stderr, "Total memory = size_kb × threads (×2 for copy: src + dst).\n");
    fprintf(stderr, "\n");
    fprintf(stderr, "Compile with -DUSE_NUMA -lnuma for explicit NUMA allocation.\n");
}

config_action_t config_parse(int argc, char *argv[], bench_config_t *cfg) {
    *cfg = (bench_config_t){
        .benchmark_tries = DEFAULT_BENCHMARK_TRIES,
        .max_runtime = DEFAULT_MAX_RUNTIME,
        .ops_mask = OP_MASK_ALL,
    };

    int opt;
    int ops_specified = 0;  /* Track if -o was used */

    while ((opt = getopt(argc, argv, "hvfas:t:r:p:o:VHR")) != -1) {
        switch (opt) {
            case 'h':
                usage(argv[0]);
                return CONFIG_EXIT_SUCCESS;
            case 'V':
                printf("%s\n", VERSION);
                return CONFIG_EXIT_SUCCESS;
            case 'v':
                cfg->verbose++;
                break;
            case 'f':
                cfg->full_sweep = 1;
                break;
            case 'a':
                cfg->auto_scaling = 1;
                break;
            case 'r':
                cfg->benchmark_tries = atoi(optarg);
                if (cfg->benchmark_tries < 1) cfg->benchmark_tries = 1;
                break;
            case 'p':
                cfg->explicit_threads = atoi(optarg);
                if (cfg->explicit_threads < 1) {
                    fprintf(stderr, "Invalid thread count: %s\n", optarg);
                    return CONFIG_EXIT_FAILURE;
                }
                break;
            case 's': {
                long size_kb = atol(optarg);
                if (size_kb <= 0) {
                    fprintf(stderr, "Invalid size: %s\n", optarg);
                    return CONFIG_EXIT_FAILURE;
                }
                cfg->single_size = (size_t)size_kb * 1024;  /* Convert KB to bytes */
                break;
            }
            case 't':
                cfg->max_runtime = atof(optarg);
                if (cfg->max_runtime < 0) {
                    fprintf(stderr, "Invalid runtime: %s (use 0 for unlimited)\n", optarg);
                    return CONFIG_EXIT_FAILURE;
                }
                break;
            case 'o': {
                /* First -o clears the default "all" mask */
                if (!ops_specified) {
                    cfg->ops_mask = 0;
                    ops_specified = 1;
                }
                /* Parse operation name */
                if (strcmp(optarg, "read") == 0) {
                    cfg->ops_mask |= (1 << OP_READ);
                } else if (strcmp(optarg, "write") == 0) {
                    cfg->ops_mask |= (1 << OP_WRITE);
                } else if (strcmp(optarg, "copy") == 0) {
                    cfg->ops_mask |= (1 << OP_COPY);
                } else if (strcmp(optarg, "latency") == 0) {
                    cfg->ops_mask |= (1 << OP_LATENCY);
                } else {
                    fprintf(stderr, "Invalid operation: %s (use: read, write, copy, latency)\n", optarg);
                    return CONFIG_EXIT_FAILURE;
                }
                break;
            }
            case 'H':
                cfg->use_hugepages = 1;
                break;
            case 'R':
                cfg->human_readable = 1;
                break;
            default:
                usage(argv[0]);
                return CONFIG_EXIT_FAILURE;
        }
    }

    return CONFIG_SUCCESS;
}

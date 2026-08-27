/*
 * sc-membench - Portable Memory Bandwidth and Latency Benchmark
 *
 * A multi-platform memory benchmark that:
 * - Works on Linux, macOS, FreeBSD, and other Unix-like systems
 * - Works on x86, arm64, and other architectures
 * - Measures read, write, and copy bandwidth using OpenMP
 * - Measures memory latency using pointer chasing
 * - Handles NUMA automatically (works on non-NUMA too)
 * - Sweeps through cache and memory sizes
 * - Finds optimal thread count for peak bandwidth
 * - Outputs CSV format for analysis
 *
 * Compile (recommended - use make for auto-detection):
 *   make              # Auto-detect available features
 *   make basic        # Minimal build, no optional dependencies
 *   make full         # All features (Linux: hwloc + numa + hugetlbfs)
 *
 * Usage:
 *   ./membench [options]
 *   ./membench -h   # Show help
 *
 * Copyright 2026 Spare Cores
 * Licensed under Mozilla Public License 2.0
 */

#include <stdlib.h>
#include <time.h>

#include "config.h"
#include "membench.h"
#include "platform.h"
#include "runner.h"

const char *const OP_NAMES[] = {"read", "write", "copy", "latency"};

int main(int argc, char *argv[]) {
    bench_config_t cfg;
    switch (config_parse(argc, argv, &cfg)) {
        case CONFIG_EXIT_SUCCESS: return 0;
        case CONFIG_EXIT_FAILURE: return 1;
        case CONFIG_SUCCESS: break;
    }

    srand((unsigned int)time(NULL));  /* Seed RNG for pointer chain randomization */

    platform_info_t platform_info;
    platform_init(&platform_info, cfg.verbose);

    run_all_benchmarks(&platform_info, &cfg);

    platform_deinit();

    return 0;
}

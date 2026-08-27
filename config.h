/*
 * Copyright 2026 Spare Cores
 * Licensed under Mozilla Public License 2.0
 */

#ifndef MEMBENCH_CONFIG_H
#define MEMBENCH_CONFIG_H

#include <stddef.h>

/* Number of times to run each benchmark, taking best result (like lmbench TRIES=11) */
#define DEFAULT_BENCHMARK_TRIES 3

/* Default total runtime target (seconds). 0 = unlimited */
#define DEFAULT_MAX_RUNTIME 0

/* Operation selection bitmask (bit 0=read, 1=write, 2=copy, 3=latency) */
#define OP_MASK_ALL 0x0F  /* All operations enabled */

typedef struct {
    int verbose;
    int full_sweep;
    size_t single_size;
    int human_readable;
    int benchmark_tries;
    int explicit_threads;
    int auto_scaling;
    double max_runtime;
    int use_hugepages;
    int ops_mask;
} bench_config_t;

typedef enum {
    CONFIG_SUCCESS,           /* arguments accepted: run the benchmark */
    CONFIG_EXIT_SUCCESS,  /* -h or -V handled: exit 0 */
    CONFIG_EXIT_FAILURE   /* invalid argument reported on stderr: exit 1 */
} config_action_t;

config_action_t config_parse(int argc, char *argv[], bench_config_t *cfg);

#endif /* MEMBENCH_CONFIG_H */

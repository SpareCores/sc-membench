/*
 * Copyright 2026 Spare Cores
 * Licensed under Mozilla Public License 2.0
 */

#ifndef MEMBENCH_H
#define MEMBENCH_H

#include <stddef.h>

#define VERSION "1.2.2"

/* Target time per individual measurement (seconds) */
#define TARGET_TIME_PER_TEST 0.25

/* Minimum iterations per test (keep low for large buffers that take seconds per iteration) */
#define MIN_ITERATIONS 3

/* Maximum iterations per test */
#define MAX_ITERATIONS 10000000

/* Fixed RAM sizes for when we need to measure pure memory bandwidth */
#define RAM_SIZE_1 (64UL * 1024 * 1024)   /* 64 MB - definitely past any L3 */
#define RAM_SIZE_2 (256UL * 1024 * 1024)  /* 256 MB - more RAM data points */

typedef enum {
    OP_READ,
    OP_WRITE,
    OP_COPY,
    OP_LATENCY   /* Memory latency test using pointer chasing */
} operation_t;

extern const char *const OP_NAMES[];  /* indexed by operation_t, defined in membench.c */

typedef struct {
    size_t size;
    operation_t op;
    int threads;
    double bandwidth_mb_s;  /* For read/write/copy */
    double latency_ns;      /* For latency test (median) */
    double latency_mean_ns; /* For latency test (mean) */
    double latency_stddev_ns; /* For latency test (standard deviation) */
    double latency_cv;      /* Coefficient of variation (stddev/mean) */
    int latency_samples;    /* Number of samples for latency measurement */
    double elapsed_s;
    int iterations;
} result_t;

#endif /* MEMBENCH_H */

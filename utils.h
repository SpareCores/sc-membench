/*
 * Copyright 2026 Spare Cores
 * Licensed under Mozilla Public License 2.0
 */

#ifndef MEMBENCH_UTILS_H
#define MEMBENCH_UTILS_H

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <time.h>

/* Monotonic wall-clock time in seconds */
static inline double get_time(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}

/* Comparison function for qsort (double ascending) */
static inline int compare_double(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/* Calculate median of sorted array */
static inline double calculate_median(double *sorted, int n) {
    if (n == 0) return 0;
    if (n % 2 == 0) {
        return (sorted[n/2 - 1] + sorted[n/2]) / 2.0;
    }
    return sorted[n/2];
}

/* Calculate mean of array */
static inline double calculate_mean(double *arr, int n) {
    if (n == 0) return 0;
    double sum = 0;
    for (int i = 0; i < n; i++) {
        sum += arr[i];
    }
    return sum / n;
}

/* Calculate standard deviation of array */
static inline double calculate_stddev(double *arr, int n, double mean) {
    if (n < 2) return 0;
    double sum_sq = 0;
    for (int i = 0; i < n; i++) {
        double diff = arr[i] - mean;
        sum_sq += diff * diff;
    }
    return sqrt(sum_sq / (n - 1));  /* Sample standard deviation */
}

/* Round size to nearest power of 2 for cleaner output */
static inline size_t round_to_power_of_2(size_t size) {
    if (size == 0) return 4096;
    size_t power = 1;
    while (power < size) power <<= 1;
    /* Return closer of power and power/2 */
    if (power - size > size - power/2 && power/2 >= 4096) {
        return power / 2;
    }
    return power;
}

/* Format size for human readable output (e.g., 1024 KB -> "1 MB") */
static inline const char* format_size(size_t size_kb, char *buf, size_t buf_size) {
    if (size_kb >= 1024 * 1024) {
        snprintf(buf, buf_size, "%zu GB", size_kb / (1024 * 1024));
    } else if (size_kb >= 1024) {
        snprintf(buf, buf_size, "%zu MB", size_kb / 1024);
    } else {
        snprintf(buf, buf_size, "%zu KB", size_kb);
    }
    return buf;
}

/* Format bandwidth for human readable output */
static inline const char* format_bandwidth(double mb_s, char *buf, size_t buf_size) {
    if (mb_s >= 1000000) {
        snprintf(buf, buf_size, "%.1f TB/s", mb_s / 1000000);
    } else if (mb_s >= 1000) {
        snprintf(buf, buf_size, "%.1f GB/s", mb_s / 1000);
    } else {
        snprintf(buf, buf_size, "%.1f MB/s", mb_s);
    }
    return buf;
}

#endif /* MEMBENCH_UTILS_H */

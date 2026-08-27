/*
 * Copyright 2026 Spare Cores
 * Licensed under Mozilla Public License 2.0
 */

#ifndef MEMBENCH_REPORT_H
#define MEMBENCH_REPORT_H

#include <stddef.h>
#include <stdio.h>

#include "config.h"
#include "membench.h"
#include "platform.h"
#include "utils.h"

/* Summary statistics structure */
typedef struct {
    /* Peak bandwidth for large buffer sizes (RAM speed) */
    double peak_read_mb_s;
    double peak_write_mb_s;
    double peak_copy_mb_s;

    /* Best latency for large buffer sizes (RAM latency) */
    double best_latency_ns;

    /* Weighted average bandwidth (larger sizes weighted more) */
    double weighted_avg_read_mb_s;
    double weighted_avg_write_mb_s;
    double weighted_avg_copy_mb_s;

    /* Counts and weights for weighted average */
    double read_weight_sum;
    double write_weight_sum;
    double copy_weight_sum;
    double read_bw_weighted_sum;
    double write_bw_weighted_sum;
    double copy_bw_weighted_sum;

    /* Track the largest size tested for "RAM" results */
    size_t largest_size_tested;

    /* Count of measurements */
    int read_count;
    int write_count;
    int copy_count;
    int latency_count;
} summary_t;

static summary_t g_summary = {0};

static void print_csv_header(const bench_config_t *cfg) {
    if (cfg->human_readable) {
        printf("\n%-10s %-8s %12s %12s %8s\n",
               "Size", "Op", "Bandwidth", "Latency", "Threads");
        printf("%-10s %-8s %12s %12s %8s\n",
               "----", "--", "---------", "-------", "-------");
    } else {
        printf("size_kb,operation,bandwidth_mb_s,latency_ns,latency_stddev_ns,latency_samples,threads,iterations,elapsed_s\n");
    }
}

static void print_result(const bench_config_t *cfg, const result_t *r) {
    size_t size_kb = r->size / 1024;

    if (cfg->human_readable) {
        char size_buf[32], bw_buf[32];
        format_size(size_kb, size_buf, sizeof(size_buf));

        if (r->op == OP_LATENCY) {
            printf("%-10s %-8s %12s %9.1f ns %8d\n",
                   size_buf, OP_NAMES[r->op], "-", r->latency_ns, r->threads);
        } else {
            format_bandwidth(r->bandwidth_mb_s, bw_buf, sizeof(bw_buf));
            printf("%-10s %-8s %12s %12s %8d\n",
                   size_buf, OP_NAMES[r->op], bw_buf, "-", r->threads);
        }
    } else {
        if (r->op == OP_LATENCY) {
            /* For latency test: report median, stddev, and sample count for statistical validity
             * Median is robust to outliers and provides reliable central tendency
             * StdDev indicates measurement precision
             * Sample count shows measurement effort */
            printf("%zu,%s,0,%.2f,%.2f,%d,%d,%d,%.6f\n",
                   size_kb, OP_NAMES[r->op], r->latency_ns,
                   r->latency_stddev_ns, r->latency_samples,
                   r->threads, r->iterations, r->elapsed_s);
        } else {
            /* For bandwidth tests, latency fields are 0 */
            printf("%zu,%s,%.2f,0,0,0,%d,%d,%.6f\n",
                   size_kb, OP_NAMES[r->op], r->bandwidth_mb_s,
                   r->threads, r->iterations, r->elapsed_s);
        }
    }
}

/* Update summary statistics with a new result */
static void update_summary(const result_t *r) {
    /* Weight by log2 of size - larger sizes get more weight */
    double weight = log2((double)r->size / 1024.0 + 1.0);

    /* Track largest size tested */
    if (r->size > g_summary.largest_size_tested) {
        g_summary.largest_size_tested = r->size;
    }

    switch (r->op) {
        case OP_READ:
            g_summary.read_count++;
            if (r->bandwidth_mb_s > g_summary.peak_read_mb_s) {
                g_summary.peak_read_mb_s = r->bandwidth_mb_s;
            }
            g_summary.read_bw_weighted_sum += r->bandwidth_mb_s * weight;
            g_summary.read_weight_sum += weight;
            break;

        case OP_WRITE:
            g_summary.write_count++;
            if (r->bandwidth_mb_s > g_summary.peak_write_mb_s) {
                g_summary.peak_write_mb_s = r->bandwidth_mb_s;
            }
            g_summary.write_bw_weighted_sum += r->bandwidth_mb_s * weight;
            g_summary.write_weight_sum += weight;
            break;

        case OP_COPY:
            g_summary.copy_count++;
            if (r->bandwidth_mb_s > g_summary.peak_copy_mb_s) {
                g_summary.peak_copy_mb_s = r->bandwidth_mb_s;
            }
            g_summary.copy_bw_weighted_sum += r->bandwidth_mb_s * weight;
            g_summary.copy_weight_sum += weight;
            break;

        case OP_LATENCY:
            g_summary.latency_count++;
            /* For latency, track the largest buffer size tested for the most RAM-like result */
            if (r->latency_ns > 0 && r->size >= g_summary.largest_size_tested) {
                /* Always update with the largest size measurement */
                g_summary.best_latency_ns = r->latency_ns;
            }
            break;
    }
}

/* Print summary statistics */
static void print_summary(const platform_info_t *pi, const bench_config_t *cfg) {
    fprintf(stderr, "\n");
    fprintf(stderr, "================================================================================\n");
    fprintf(stderr, "                           BENCHMARK SUMMARY\n");
    fprintf(stderr, "================================================================================\n\n");

    /* Calculate weighted averages */
    if (g_summary.read_weight_sum > 0) {
        g_summary.weighted_avg_read_mb_s = g_summary.read_bw_weighted_sum / g_summary.read_weight_sum;
    }
    if (g_summary.write_weight_sum > 0) {
        g_summary.weighted_avg_write_mb_s = g_summary.write_bw_weighted_sum / g_summary.write_weight_sum;
    }
    if (g_summary.copy_weight_sum > 0) {
        g_summary.weighted_avg_copy_mb_s = g_summary.copy_bw_weighted_sum / g_summary.copy_weight_sum;
    }

    /* Print bandwidth results */
    fprintf(stderr, "BANDWIDTH (MB/s):\n");
    fprintf(stderr, "  %-10s %12s %12s\n", "Operation", "Peak", "Weighted Avg");
    fprintf(stderr, "  %-10s %12s %12s\n", "---------", "----", "------------");

    if (g_summary.read_count > 0) {
        fprintf(stderr, "  %-10s %12.0f %12.0f\n", "Read",
                g_summary.peak_read_mb_s, g_summary.weighted_avg_read_mb_s);
    }
    if (g_summary.write_count > 0) {
        fprintf(stderr, "  %-10s %12.0f %12.0f\n", "Write",
                g_summary.peak_write_mb_s, g_summary.weighted_avg_write_mb_s);
    }
    if (g_summary.copy_count > 0) {
        fprintf(stderr, "  %-10s %12.0f %12.0f\n", "Copy",
                g_summary.peak_copy_mb_s, g_summary.weighted_avg_copy_mb_s);
    }

    /* Print latency results */
    if (g_summary.latency_count > 0 && g_summary.best_latency_ns > 0) {
        fprintf(stderr, "\nLATENCY:\n");
        const char *cache_note = "";
        if (g_summary.largest_size_tested < 1024 * 1024) {
            cache_note = " (L2/L3 cache)";
        } else if (g_summary.largest_size_tested < 64 * 1024 * 1024) {
            cache_note = " (L3 cache/RAM)";
        } else {
            cache_note = " (RAM)";
        }
        fprintf(stderr, "  Best latency: %.1f ns%s at %zu KB buffer\n",
                g_summary.best_latency_ns, cache_note, g_summary.largest_size_tested / 1024);
    }

    /* Calculate and print composite benchmark score
     * Score formula: geometric mean of bandwidth scores, divided by latency factor
     * Higher is better for all components */
    fprintf(stderr, "\n");
    fprintf(stderr, "--------------------------------------------------------------------------------\n");
    fprintf(stderr, "BENCHMARK SCORE (higher is better):\n\n");

    /* Individual scores */
    double bw_total = 0;
    int bw_count = 0;

    if (g_summary.peak_read_mb_s > 0) {
        bw_total += g_summary.peak_read_mb_s;
        bw_count++;
    }
    if (g_summary.peak_write_mb_s > 0) {
        bw_total += g_summary.peak_write_mb_s;
        bw_count++;
    }
    if (g_summary.peak_copy_mb_s > 0) {
        bw_total += g_summary.peak_copy_mb_s;
        bw_count++;
    }

    /* Bandwidth score: average of peak bandwidths (in GB/s for nicer numbers) */
    double bw_score = 0;
    if (bw_count > 0) {
        bw_score = (bw_total / bw_count) / 1000.0;  /* Convert MB/s to GB/s */
        fprintf(stderr, "  Bandwidth Score:    %8.1f  (avg peak bandwidth in GB/s)\n", bw_score);
    }

    /* Latency score: inverse of latency (higher = faster memory) */
    double latency_score = 0;
    if (g_summary.best_latency_ns > 0) {
        latency_score = 1000.0 / g_summary.best_latency_ns;  /* 1000/ns gives reasonable scale */
        fprintf(stderr, "  Latency Score:      %8.1f  (1000 / latency_ns)\n", latency_score);
    }

    /* Combined score: geometric mean if both available, otherwise just bandwidth */
    double combined_score = 0;
    if (bw_score > 0 && latency_score > 0) {
        combined_score = sqrt(bw_score * latency_score) * 100;  /* Scale for nice numbers */
        fprintf(stderr, "\n  >> COMBINED SCORE:  %8.0f  (sqrt(bw_score × latency_score) × 100)\n", combined_score);
    } else if (bw_score > 0) {
        combined_score = bw_score * 100;
        fprintf(stderr, "\n  >> COMBINED SCORE:  %8.0f  (bandwidth only, no latency data)\n", combined_score);
    }

    fprintf(stderr, "--------------------------------------------------------------------------------\n");

    /* Warn if options that affect score comparability were used */
    int has_warnings = 0;
    if (cfg->max_runtime > 0 || cfg->explicit_threads > 0 || cfg->single_size > 0) {
        fprintf(stderr, "\n");
        fprintf(stderr, "WARNING: Scores may not be comparable due to non-default options:\n");
        if (cfg->max_runtime > 0) {
            fprintf(stderr, "  - Time limit (-t %.0f) may have prevented testing larger buffer sizes\n", cfg->max_runtime);
            has_warnings = 1;
        }
        if (cfg->explicit_threads > 0) {
            fprintf(stderr, "  - Fixed thread count (-p %d) instead of using all CPUs (%d)\n",
                    cfg->explicit_threads, pi->num_cpus);
            has_warnings = 1;
        }
        if (cfg->single_size > 0) {
            fprintf(stderr, "  - Single buffer size (-s %zu KB) instead of full sweep\n",
                    cfg->single_size / 1024);
            has_warnings = 1;
        }
        if (has_warnings) {
            fprintf(stderr, "For comparable scores, run without -t, -p, or -s options.\n");
        }
    }
    fprintf(stderr, "\n");
}

#endif /* MEMBENCH_REPORT_H */

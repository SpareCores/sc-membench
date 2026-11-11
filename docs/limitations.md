# Limitations

## What This Benchmark Does *Not* Deliver

### Plain C Loops

The read, write, and copy loops are plain C, compiled with `-O3` for the
baseline instruction set of each architecture (`-march=x86-64` or
`-mcpu=generic+crc`, set in the [`Makefile`](../Makefile)). One binary
therefore runs on every CPU of the architecture.

The loops contain no hand-written SIMD (Single Instruction, Multiple Data)
code and no non-temporal stores. Every server type runs the same portable
code, so the results show the bandwidth of that code, not of loops tuned for
one CPU.

### Container Limits

The tool reads the CPU count and the RAM size of the host, not the limits of
the container. Docker's `--cpus` and `--memory` options do not change the
thread count, the [sweep limit](./workloads.md#buffer-sizes), or the memory
guard.

### Bandwidth Unit

The code divides bytes by 1,048,576, so `bandwidth_mb_s` is in mebibytes per
second (MiB/s). The column name, the usage text, and the label in
[Spare Cores Navigator](https://docs.sparecores.com/overview) call it MB/s. The
`-R` table divides the MiB/s value by 1,000 for each larger label, so a value
shown as "GB/s" is in units of 1,000 MiB/s.

The `size_kb` column is in KiB (bytes divided by 1,024).

## Scope Decisions

### Cache Detection

The sweep follows the caches that `hwloc` (Portable Hardware Locality)
reports, so its sizes are only as good as that detection:

- The L3 size is that of the first L3 cache object, which is one shared L3
  cache. On a server with several L3 caches, the largest sweep size, 4 × L3,
  is four times one of them, not four times their total. With `-v`, stderr labels the size
  "(per core)", which it is not.
- When `hwloc` finds no caches, the sizes follow the defaults of 32 KiB,
  256 KiB, and 8 MiB instead of the real caches. In
  [production](./usage.md#production-setup), this happens on 17 server types
  with Ampere Altra-class Arm CPUs. The log does not mark
  the fallback: it prints the defaults as detected sizes.

See [Cache Detection](./workloads.md#cache-detection) for examples.

### Aggregate Bandwidth

Each thread has its own buffer, and the bandwidth is the total bytes of all
threads divided by the time of the slowest thread. It is not a sum of
per-thread bandwidths, and this benchmark reports no per-thread or per-node
value. Copy counts the buffer size once, as `bw_mem` does.

### Thread Placement

The bandwidth threads run under the OpenMP `proc_bind(spread)` clause, and
the image sets neither `OMP_PLACES` nor `OMP_PROC_BIND`. The following points
describe what the code does and does not control:

- `OMP_NUM_THREADS` and `OMP_PROC_BIND` have no effect on the bandwidth
  operations, although the usage text of the tool lists them.
- The map of CPUs to NUMA (Non-Uniform Memory Access) nodes that `-v` prints
  is not used to place threads.
- The latency test pins the main thread to CPU 0 and does not restore its
  affinity, so OpenMP thread 0 stays on CPU 0 in the bandwidth measurements
  that follow.

## Design Constraints

### Memory Guard

The membench memory guard applies at sizes where the buffers of all threads
would exceed a quarter of the RAM: it halves the thread count until they fit.
Rows of one run can therefore have
different `threads` values, and their bandwidths are not directly comparable.
In production, 453 runs have at least one bandwidth row with fewer threads
than the run's largest thread count. Compare the `threads` column before
comparing bandwidths at large sizes.

### Latency Sampling

The latency test has the following constraints:

- Sampling stops at 21 samples whatever the CV (Coefficient of Variation), so
  a row with 21 samples can have a CV above the 5% target. For example, the
  16,384 KiB row of an AWS (Amazon Web Services) `c8g.medium` run reports
  24.76 ns with a standard deviation of 4.03 ns, a CV of about 16%
  ([recorded output](https://github.com/SpareCores/sc-inspector-data/blob/main/data/aws/c8g.medium/0/membench/stdout)).
- The chain is capped at 2 GiB or a quarter of the RAM, whichever is smaller,
  but `size_kb` still shows the size of the sweep. The default sweep ends at
  4 × L3, so the cap matters mainly with `-f`.
- Latency is measured once per size, on CPU 0 only.

### Huge Pages Scope

With `libnuma` available, as in the image, multi-thread bandwidth buffers
come from `numa_alloc_onnode` and never use huge pages. The `-H` option then
changes only the latency rows and the bandwidth rows whose `threads` value is
1. A bandwidth run uses one thread in the following cases:

- on a server with 1 vCPU
- with `-p 1`
- in the first step of `-a`
- when the memory guard halves the thread count down to 1

The huge page size comes from `/proc/meminfo`; the image is built without
`libhugetlbfs`.

### Time Limit and Exit Code

The `-t` limit is soft: it is checked only after each measurement ends, so a
run can pass it by the length of one measurement. It is ignored with `-s`.
The tool handles no signals.

The exit code is 0 after every run that starts measuring; an invalid or
unknown option exits with 1 before that. A measurement whose buffers cannot
be allocated prints no row. Only `-v` logs a failed bandwidth allocation; a
failed latency allocation is always logged.

To find a dropped measurement, run with `-v` and compare the output with the
log. The `Testing N buffer sizes` line gives the number of sizes, and the
`Cache` line gives the cache sizes from which the table in
[Buffer Sizes](./workloads.md#buffer-sizes) derives them. A size with fewer
rows than the operations selected (four by default), or fewer sizes than
that line announces, marks a dropped measurement or a stop at the `-t`
limit, which the log reports as `Time limit reached`.

### Older Production Results

Production results from versions 1.0.1 to 1.2.1 are published next to those
of 1.2.2, with the version recorded in the `version` field of each run's
`meta.json`. Version 1.0.3 changed the latency method, and 1.1.0 moved the
bandwidth operations to OpenMP. Five of the oldest runs have a CSV
(Comma-Separated Values) with seven columns, without `latency_stddev_ns` and
`latency_samples`.

We plan to rerun the server types that have older results, with no date set
yet. Until then, check the version of each result before comparing two server
types.

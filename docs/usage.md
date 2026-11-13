# Usage

## Running via Docker

With no arguments, the image sweeps the default sizes with all four operations
on all CPUs, and prints a CSV to stdout:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main
```

The container entrypoint is
`resource-tracker -- nice -n -20 /usr/local/bin/membench`
([`Dockerfile`](../Dockerfile)), where the `resource-tracker` binary comes from
the
[`resource-tracker` image](https://github.com/SpareCores/sc-images/tree/main/images/resource-tracker).
Arguments after the image name go to `membench`. The `--privileged` option is
required, because raising the priority with `nice -n -20` needs
`CAP_SYS_NICE`.

The following examples show the common variations. To log the detected system
to stderr and stop after about 5 minutes:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main -v -t 300
```

To test one per-thread buffer size of 1,024 KiB:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main -s 1024
```

To use huge pages, with a log on stderr:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main -H -v
```

To run only the latency test, with huge pages:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main -H -o latency
```

To save the CSV to a file:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main -H > results.csv
```

To keep the CSV and the log in separate files:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main -Hv > results.csv 2> membench.log
```

To print the version of the tool:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main -V
```

The image is published for 64-bit x86 servers with Intel or AMD CPUs
(`linux/amd64`) and 64-bit Arm servers, such as AWS Graviton (`linux/arm64`),
and rebuilt on every push to the `main` branch of the
[`sc-membench` repository](https://github.com/SpareCores/sc-membench). Use the
`:main` tag; `:latest` points to the same build.

A published GitHub release also pushes a tag with the release version, but
the last release predates the image build, so the image has no version tag
yet. The `:main` image runs version
[1.2.2](../CHANGELOG.md#122-man-page) of the code, which `-V` prints.

A run writes to stderr only with `-v` or `-R`, or when it fails to allocate a
latency buffer. With `-v`, the log looks as follows on an AWS (Amazon Web
Services) [`m7i.48xlarge`](https://sparecores.com/server/aws/m7i.48xlarge)
server ([recorded
log](https://github.com/SpareCores/sc-inspector-data/blob/main/data/aws/m7i.48xlarge/0/membench/stderr)):

<!--TW note: add embed?-->

```text
System: 192 CPUs, 743.94 GB memory
Note: 2 L3 caches detected (distributed across dies)
Cache (hwloc): L1d=48KB, L2=2048KB, L3=107520KB (per core)
Minimum total test size: 3072 KB (16KB × 192 CPUs)
NUMA: 2 nodes detected (libnuma enabled)
NUMA topology:
  Node 0: 96 CPUs (first: 0, last: 143)
  Node 1: 96 CPUs (first: 48, last: 191)
Testing 10 buffer sizes (per thread, adaptive to cache hierarchy)
Thread mode: num_cpus (192 threads)
OpenMP: proc_bind(spread) for NUMA-aware thread placement
Total runtime: 350.7 seconds
```

Note that the L3 size on the `Cache` line is the size of one shared L3 cache,
although the line labels all three sizes "(per core)", and that the "Minimum
total test size" is printed but not used. The "KB" of the cache sizes is KiB:
the code divides bytes by 1,024
([`membench.c`](../membench.c#L979-L982)). See
[Cache Detection](./workloads.md#cache-detection) for what the sizes mean.

## Running Without Docker

Install `membench` from a distribution package, or build it from source:

- **Package:** see [Packages](./build.md#packages) for the packaging status
  of `sc-membench` across distributions.
- **Source:** see [Building From Source](./build.md) for the dependencies per
  platform and the build targets, and
  [Recommended Build](./build.md#recommended-build) for the commands on a
  server.

The binary takes the same options as the image. To run with the default
settings, which use all CPUs and sizes derived from the caches:

```bash
./membench
```

To log to stderr and stop after about 5 minutes (300 seconds):

```bash
./membench -v -t 300
```

To test one per-thread buffer size of 1,024 KiB (1 MiB):

```bash
./membench -s 1024
```

To run the binary built with `make numa`, with a log:

```bash
./membench-numa -v
```

Outside the container, the run has no `nice -n -20` priority and no
`resource-tracker`. To match the priority, run the binary as root under
`nice -n -20`.

## Usage Examples

The following commands use the native binary; with the image, put the same
options after the image name, as in [Running via Docker](#running-via-docker).
See [Options](#options) for every option and its default.

To use huge pages for large buffers, which is recommended for benchmarking
(with `libnuma`, only the latency chain and one-thread bandwidth rows use
them):

```bash
./membench -H
```

To use huge pages with a log that shows the detected system and the runtime:

```bash
./membench -H -v
```

To print a human-readable table, a summary, and scores instead of a CSV:

```bash
./membench -R
```

To run each bandwidth measurement 5 times instead of 3, and keep the best:

```bash
./membench -r 5
```

To run each bandwidth measurement once, for the shortest runtime:

```bash
./membench -r 1
```

To use exactly 16 threads for the bandwidth operations:

```bash
./membench -p 16
```

To try 1, 2, 4, and so on up to the CPU count, and keep the best per size:

```bash
./membench -a
```

To extend the sweep with 512 MiB, 1 GiB, and larger sizes up to the sweep
limit:

```bash
./membench -f
```

To run only the read and latency operations:

```bash
./membench -o read -o latency
```

To bind each thread to a whole core instead of a hardware thread (OpenMP
places):

```bash
OMP_PLACES=cores ./membench
```

`OMP_PROC_BIND` and `OMP_NUM_THREADS` do not change the bandwidth operations;
see [Environment Variables](#environment-variables).

## Explicit Huge Pages

With `-H`, a buffer of at least twice the huge page size first tries explicit
huge pages, then THP (Transparent Huge Pages), then regular pages; see
[Huge Pages](./workloads.md#huge-pages). THP takes effect only when the
host's THP mode, in `/sys/kernel/mm/transparent_hugepage/enabled`, is
`madvise` or `always`, and the tool does not check that mode. Explicit huge
pages come from a pool that must be pre-allocated, and when that pool runs
out, the buffer falls back to THP.

The `-vv` option logs the huge page path of each allocation. For THP, it
shows only that the `madvise` call succeeded, not that the kernel used huge
pages.

[Production](#production-setup) passes `-H`, but whether its hosts provide
explicit huge pages or THP is not published, so the page type behind the
published latency results is unknown, and a run of the image may get a
different one.

With `libnuma`, as in the image, in `make numa` and `make full` builds, and in a
plain `make` build that detects it, multi-thread bandwidth buffers never use
huge pages; see [Huge Pages Scope](./limitations.md#huge-pages-scope). The pool
then only has to hold the latency chain, which is capped at 2 GiB or a quarter
of the RAM, whichever is smaller. When the thread count is 1, the bandwidth
buffers also come from the pool: the size, and twice that for copy.

To pre-allocate explicit huge pages of 2 MiB for the latency test, first
check the current huge page status:

```bash
grep Huge /proc/meminfo
```

The latency test runs on one thread, on a chain of at most 2 GiB. A 256 MiB
buffer needs 256 / 2 = 128 pages (256 MiB), and the 2 GiB chain of the cap
needs 2,048 / 2 = 1,024 pages (2 GiB).

To allocate the 1,024 pages of the 2 GiB cap as root (replace the count with
the one for the host):

```bash
echo 1024 | sudo tee /proc/sys/vm/nr_hugepages
```

To run with huge pages, which uses the explicit huge pages if they are
available:

```bash
./membench -H -v
```

A binary built without `libnuma` also takes its multi-thread bandwidth
buffers from the pool. The number of huge pages that the bandwidth tests
(read, write, and copy) need is threads × buffer size × 2 / 2 MiB, as copy
uses a source and a destination buffer. For example, with a 256 MiB buffer:

- 8 CPUs: 8 × 256 × 2 / 2 = 2,048 pages (4 GiB)
- 64 CPUs: 64 × 256 × 2 / 2 = 16,384 pages (32 GiB)
- 192 CPUs: 192 × 256 × 2 / 2 = 49,152 pages (96 GiB)

To allocate the 49,152 pages of the 192-CPU example as root (replace the count
with the one for the host):

```bash
echo 49152 | sudo tee /proc/sys/vm/nr_hugepages
```

With `libnuma`, skip this sizing: the latency pool above is all a run
needs.

## Run Duration

Production runs of version [1.2.2](../CHANGELOG.md#122-man-page) whose
`meta.json` records an `exit_code` of 0 (2,317 runs) took the following times,
read from the `Total runtime` line of their stderr:

| Run | Duration | Server type |
| --- | --- | --- |
| Fastest | 28.0s | [`aws/c8a.medium`](https://sparecores.com/server/aws/c8a.medium) ([recorded log](https://github.com/SpareCores/sc-inspector-data/blob/main/data/aws/c8a.medium/0/membench/stderr)) |
| Median | 130.6s | |
| 90th percentile | 320.9s | |
| Slowest | 914.3s | [`gcp/c4-highmem-288`](https://sparecores.com/server/gcp/c4-highmem-288) ([recorded log](https://github.com/SpareCores/sc-inspector-data/blob/main/data/gcp/c4-highmem-288/0/membench/stderr)) |

The longest run of any version took 1,428.6 seconds, on
[`aws/i7ie.48xlarge`](https://sparecores.com/server/aws/i7ie.48xlarge) with
version [1.0.2](../CHANGELOG.md#102-more-consistent-results) ([recorded
log](https://github.com/SpareCores/sc-inspector-data/blob/main/data/aws/i7ie.48xlarge/0/membench/stderr)).
No production run reached the limit of 1,800 seconds.

Before each measurement, one thread times a single pass over its buffer and
sets the iteration count, the number of passes, so that one thread would need
about 0.25 seconds for them (see [Calibration](./workloads.md#calibration)).
All threads then run that count.
When the threads together are limited by memory bandwidth, one multi-thread
measurement can therefore take several seconds: `elapsed_s` reaches about
4.3 seconds in production,
on [`aws/c8g.48xlarge`](https://sparecores.com/server/aws/c8g.48xlarge)
([recorded output](https://github.com/SpareCores/sc-inspector-data/blob/main/data/aws/c8g.48xlarge/0/membench/stdout)).

Each bandwidth measurement also runs four times: one discarded warmup try and
three tries.

A latency measurement takes 7 to 21 samples of about 0.1 seconds each, and its
`elapsed_s` also includes allocating and shuffling the chain. In the
[example](../README.md#results), the 128 MiB latency row took 10.1 seconds.

Use the `Total runtime` line for the duration of this benchmark itself. The
`start` and `end` times in a run's `meta.json` include more than this
benchmark: one run took 2,219 seconds by its
[`meta.json`](https://github.com/SpareCores/sc-inspector-data/blob/main/data/alicloud/ecs.e-c1m2.2xlarge/0/membench/meta.json)
but 79.0 seconds by its stderr.

## Production Setup

The [Inspector](https://docs.sparecores.com/overview#how-it-works) of
[Spare Cores Navigator](https://docs.sparecores.com/overview) runs this image
as the `membench` task
([`tasks.py`](https://github.com/SpareCores/sc-inspector/blob/main/inspector/tasks.py)).
A plain `docker run` differs from the production setup as follows:

| Setting | Production | Plain `docker run` |
| --- | --- | --- |
| Arguments | `-Hv -t 1800`: huge pages, a log on stderr, and a soft limit of 1,800s | None: regular pages, no log, and no time limit |
| Privileges | Privileged container | Privileged only with `--privileged`, which the image requires |
| Network | Host networking, which comes from the shared `DOCKER_OPTS` of the Inspector, not from the image | Docker's default network |
| Timeout | The task is stopped after 40 min | None |
| Version | Recorded in `meta.json` from a separate `-V` call | Not recorded |
| Resource tracking | `resource-tracker` gets a project name, a job name, a run ID, and an access token for [Spare Cores Sentinel](https://sentinel.sparecores.com/) | `resource-tracker` runs with `TRACKER_QUIET=true` and none of these settings |

The equivalent command is the following:

```bash
docker run --rm --privileged --network host ghcr.io/sparecores/membench:main -Hv -t 1800 > stdout 2> stderr
```

Production runs this benchmark on every server type except those on the
blocklist in
[`inspector_rules.py`](https://github.com/SpareCores/sc-crawler/blob/main/src/sc_crawler/inspector_rules.py),
which for this benchmark excludes servers with less than 0.9 GB of memory and
`ovh/r3-128`. The image does not enforce a minimum memory size.

The results are published in the
[`sc-inspector-data` repository](https://github.com/SpareCores/sc-inspector-data)
as `data/<vendor>/<server>/<variant>/membench/`, with the files `meta.json`,
`stdout`, and `stderr`; `<variant>` is `0` for most server types. The resource
usage that `resource-tracker` samples is not published.

## CSV Output

Rows are printed to stdout as each measurement ends, one per size and
operation, after a header row that names the columns:

```text
size_kb,operation,bandwidth_mb_s,latency_ns,latency_stddev_ns,latency_samples,threads,iterations,elapsed_s
```

The columns are the following:

| Column | Meaning |
| --- | --- |
| `size_kb` | The per-thread buffer size in KiB. |
| `operation` | `read`, `write`, `copy`, or `latency`. |
| `bandwidth_mb_s` | The aggregate bandwidth of all threads in MiB/s, and `0` for latency rows. |
| `latency_ns` | The median time of one dependent load in nanoseconds, and `0` for bandwidth rows. |
| `latency_stddev_ns` | The sample standard deviation of the latency samples, and `0` for bandwidth rows. |
| `latency_samples` | The count of the latency samples (7 to 21), and `0` for bandwidth rows. |
| `threads` | The thread count used, always `1` for latency. |
| `iterations` | The number of passes over the buffer per thread in the kept try for bandwidth rows, and the sample count for latency rows. |
| `elapsed_s` | The duration of the timed loop of the slowest thread for bandwidth rows, and the wall time including chain allocation for latency rows. |

The memory in use for a row, in KiB, is `size_kb` × `threads`, and twice
that for copy. See [Results](../README.md#results) for an example.

## Reading the Results

Bandwidth drops and latency rises as the buffer size passes each cache level:

- The first change is at the L1d boundary: the L1d size on the `Cache` line
  of the `-v` log, such as 64 KiB per thread on an AWS `c8g.medium` server.
- The next is at the L2 boundary: the L2 size on the same line, such as
  2 MiB per thread on that server.
- The last comes when the memory in use exceeds the L3 cache. That point
  depends on the thread count: a bandwidth row uses `size_kb` × `threads`,
  while latency runs on one thread.

Compare the `threads` column before comparing bandwidths at large sizes; see
[Memory Guard](./limitations.md#memory-guard). Typical values given for
earlier versions, without a source, are kept in the changelog under
[Typical Values](../CHANGELOG.md#typical-values).

## How the Navigator Uses the Results

The Navigator reads only the CSV and publishes four metrics from it, each per
`size_kb`:

- `membench:bandwidth_read`, `membench:bandwidth_write`, and
  `membench:bandwidth_copy`, labeled MB/s (see
  [Bandwidth Unit](./limitations.md#bandwidth-unit))
- `membench:latency` in nanoseconds, where lower is better

For each metric, it also publishes one RAM value: the first row whose
per-thread `size_kb` exceeds the total size of the server's last cache
level, as the Navigator records it for the server type. That is the total of
the L3 caches, else of the L2 caches, else of the L1d caches, not the size
of one L3 cache that the sweep uses; see
[Cache Detection](./limitations.md#cache-detection). The comparison uses the
per-thread size, not `size_kb` × `threads`.

A RAM value is published only when a row exceeds that total and the
Navigator has a cache size for the server type.

The `-R` summary and scores are not published; production does not pass
`-R`. Results of versions before [1.2.2](../CHANGELOG.md#122-man-page) are still published, with the version
noted; see
[Older Production Results](./limitations.md#older-production-results).

## Human-Readable Output

With `-R`, `membench` prints a table to stdout instead of a CSV, and a summary
with scores to stderr:

```bash
docker run --rm --privileged ghcr.io/sparecores/membench:main -R
```

The table shows the bandwidth as "MB/s", "GB/s", or "TB/s". It divides the
MiB/s value of the CSV by 1,000 for each larger label, so a value shown as
"GB/s" is in units of 1,000 MiB/s. The summary holds the following values, of
which the last three are the membench scores:

- **Peak:** the highest bandwidth of each bandwidth operation over all
  sizes, in MiB/s (labeled "MB/s").
- **Weighted Avg:** Σ(bandwidth × w) / (Σ w) over all sizes of the
  operation, with w = log₂(`size_kb` + 1), so larger buffers count more,
  in MiB/s.
- **Best latency:** the tool's label for the latency at the largest size
  tested, the closest to RAM latency, in ns. The size picks it, not the
  lowest value.
- **Bandwidth Score** = mean of the peaks / 1,000, in units of 1,000 MiB/s
  (labeled "GB/s").
- **Latency Score** = 1,000 / best latency in ns, that is, dependent loads
  per microsecond.
- **Combined Score** = 100 × √(Bandwidth Score × Latency Score), with no
  unit. As a geometric mean, it lets neither bandwidth nor latency dominate.

A higher score is better: a higher bandwidth raises the Bandwidth Score, and a
lower best latency raises the Latency Score.

When `-t`, `-p`, or `-s` is set, the summary warns that the scores may not be
comparable, for example:

```text
WARNING: Scores may not be comparable due to non-default options:
  - Time limit (-t 60) may have prevented testing larger buffer sizes
  - Fixed thread count (-p 4) instead of using all CPUs (32)
For comparable scores, run without -t, -p, or -s options.
```

For comparable scores, run without these options, so that all sizes are
tested on all CPUs. Production does not pass `-R`, and the Navigator does not
publish these scores. An example of the full output is kept in the changelog
under [Example Human-Readable Output](../CHANGELOG.md#example-human-readable-output).

## Options

The following usage text is corrected against the code, so it differs from the
text that `-h` prints:

```text
Usage: membench [options]

Options:
  -h          Show help (to stderr) and exit
  -V          Show the version (1.2.2) and exit
  -v          Verbose log to stderr: system, caches, NUMA nodes, sizes,
              thread mode, total runtime (-vv adds allocation, huge page,
              and pinning lines)
  -s SIZE_KB  Test only this per-thread buffer size in KiB, e.g. -s 1024
              (disables the sweep; -t is ignored)
  -r TRIES    Tries per bandwidth measurement, best kept (default: 3, at
              least 1; not used for latency)
  -f          Full sweep: add 512 MiB, 1 GiB, ... up to the sweep limit
              (half of the RAM / threads; at most 18 sizes)
  -p THREADS  Use exactly this many threads for bandwidth (default: online CPUs)
  -a          Auto-scaling: try 1, 2, 4, ... up to the CPU count per size
              and bandwidth operation, keep the best
  -t SECONDS  Soft runtime limit, checked after each measurement; 0 = unlimited
              (default: 0)
  -o OP       Run only this operation: read, write, copy, or latency
              Can be given several times (default: all four)
  -H          Huge pages for buffers >= 2x the huge page size
  -R          Human-readable table (stdout) and summary with scores
              (stderr) instead of CSV
```

The text that `-h` prints is out of date on the following five points:

- The default sweep stops at 4 × L3, not at "512 MB per thread".
- The `-H` threshold is twice the huge page size, not "4MB".
- The bandwidth unit is MiB/s, not MB/s.
- `-t` is a soft limit, checked after each measurement and ignored with `-s`,
  not a "maximum runtime".
- The `OMP_NUM_THREADS` and `OMP_PROC_BIND` variables do not change the thread
  count or the placement.

The man page, `membench.1`, repeats the statements about the default sweep, the
`-H` threshold, and the `OMP_*` variables. For which buffers `-H` covers, see
[Huge Pages](./workloads.md#huge-pages).

An invalid value of `-p`, `-s`, `-t`, or `-o` exits with code 1, as does an
unknown option. A `-r` value below 1 is set to 1 instead
([`membench.c`](../membench.c#L2284-L2287)).

## Environment Variables

| Variable | Meaning | Default |
| --- | --- | --- |
| `TRACKER_QUIET` | Silences the output of `resource-tracker`. | `true` |
| `OMP_PLACES` | The standard OpenMP list of places that threads can be bound to, such as `cores`. | Unset |
| `OMP_PROC_BIND` | No effect on the bandwidth operations: the `proc_bind(spread)` clause in the code takes precedence. | Unset |
| `OMP_NUM_THREADS` | No effect on the bandwidth operations: the code sets its own thread count; use `-p` instead. | Unset |

See [CSV Output](#csv-output) for the output columns.

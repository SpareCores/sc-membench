# Building from Source

## Packages

For the distributions that package `sc-membench` and the versions they
carry, see the
[Repology packaging status](https://repology.org/project/sc-membench/versions).

## Requirements

The tool is one C file, [`membench.c`](../membench.c), built with the
[`Makefile`](../Makefile). The build and the binary need the following:

| Library | Purpose | Platforms | Need |
| --- | --- | --- | --- |
| C11 compiler with OpenMP (GCC or Clang) | Builds the tool | All | Required to build |
| `libgomp` | OpenMP runtime for the parallel execution (`libgomp1` on Debian and Ubuntu, `libgomp` on RHEL) | All | Required at runtime |
| `hwloc` 2 | Detects the L1d, L2, and L3 cache sizes | Linux, macOS, BSD | Recommended, at build and runtime |
| `libnuma` | Allocates each buffer on the NUMA node of its thread | Linux only | Optional, at build and runtime |
| `libhugetlbfs` | Detects the huge page size | Linux only | Optional, at build and runtime |

A build with `hwloc` needs version 2.x; version 1.x has a different API and is
not supported. Without `hwloc`, the tool reads the caches from `sysctl` on
macOS and BSD, and from `/sys/devices/system/cpu/*/cache/` on Linux. Without
`libnuma`, memory is allocated without NUMA awareness, so bandwidth may be
lower on multi-socket servers. Without `libhugetlbfs`, the huge page size is read
from `/proc/meminfo`.

## Installing Dependencies

The following commands install the build dependencies, or only the runtime
libraries for a machine that runs a binary built elsewhere:

```bash
# Debian/Ubuntu - Build
apt-get install build-essential libhwloc-dev libnuma-dev libhugetlbfs-dev

# Debian/Ubuntu - Runtime only (e.g., Docker images)
apt-get install libgomp1 libhwloc15 libnuma1 libhugetlbfs-dev

# RHEL/CentOS/Fedora - Build
yum install gcc make hwloc-devel numactl-devel libhugetlbfs-devel

# RHEL/CentOS/Fedora - Runtime only
yum install libgomp hwloc-libs numactl-libs libhugetlbfs

# macOS (hwloc only, no NUMA): Homebrew LLVM clang, as Apple clang has no OpenMP
brew install llvm hwloc
export CC="$(brew --prefix llvm)/bin/clang"
export LIBRARY_PATH="$(brew --prefix llvm)/lib"

# FreeBSD (hwloc 2 required, not hwloc 1)
pkg install gmake hwloc2
```

CI ([`ci.yml`](../.github/workflows/ci.yml)) installs them as follows; on
macOS, it uses the recipe above:

```bash
# Ubuntu
sudo apt-get update && sudo apt-get install -y libhwloc-dev libnuma-dev libhugetlbfs-dev

# FreeBSD
pkg install -y gmake hwloc2
```

**Note:** On Ubuntu, CI installs `libhwloc-dev`, `libnuma-dev`, and
`libhugetlbfs-dev`, without `build-essential`.

## Build Targets

Build one of the following targets (with `gmake` on FreeBSD) in a clone of
the [`sc-membench` repository](https://github.com/SpareCores/sc-membench).
Each comment names the binary it builds:

```bash
make              # membench: links hwloc, libnuma, and libhugetlbfs when detected
make basic        # membench-basic: no optional library (caches from /sys or sysctl)
make hwloc        # membench-hwloc: hwloc 2 only (portable cache detection)
make numa         # membench-numa: libnuma only (Linux)
make full         # membench-full: hwloc + libnuma, plus libhugetlbfs if detected
                  #   (recommended for servers; hwloc only outside Linux)
make all          # every variant the platform can build
make test         # build membench and run a quick 30-second test (./membench -v -t 30)
make install      # install the binary and the man page under /usr/local
make info         # print the detected configuration
make clean        # remove the built binaries
```

Set `CC` to use another compiler. The flags are
`-O3 -Wall -Wextra -std=c11 -fopenmp`, with `-march=x86-64 -mtune=generic` on
Linux x86_64 and `-mcpu=generic+crc` on aarch64 (arm64), so that one binary
runs on every CPU of the architecture.

## Recommended Build

For use on servers, build with all features:

```bash
# Get the source
git clone https://github.com/SpareCores/sc-membench
cd sc-membench

# Install dependencies first
sudo apt-get install build-essential libhugetlbfs-dev libhwloc-dev libnuma-dev  # Debian/Ubuntu
# or: sudo yum install gcc make libhugetlbfs-devel hwloc-devel numactl-devel  # RHEL/CentOS

# Build with full features
make full
./membench-full -v
```

See [Running Without Docker](./usage.md#running-without-docker) for how to
run the binary.

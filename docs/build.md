# Building From Source

## Packages

For the distributions that package `sc-membench` and the versions they
carry, see the
[Repology packaging status](https://repology.org/project/sc-membench/versions).

## Requirements

The tool is a single C file, [`membench.c`](../membench.c), built with the
[`Makefile`](../Makefile). The build and the binary need the following.

Required, on all platforms:

- A C11 compiler with OpenMP (GCC or Clang), to build the tool.
- `libgomp`, the OpenMP runtime for the parallel execution, to run it.

Recommended, to build and run:

- `hwloc` 2 (Portable Hardware Locality), which detects the CPU cache sizes
  (L1d, L2, and L3), on Linux, macOS, and BSD (Berkeley Software
  Distribution). Version 1.x has a different API and is not supported.
  Without `hwloc`, the tool reads the caches from `sysctl` on macOS and BSD,
  and from `/sys/devices/system/cpu/*/cache/` on Linux.

Optional, to build and run, on Linux only:

- `libnuma`, which allocates each buffer on the NUMA (Non-Uniform Memory
  Access) node of its thread. Without `libnuma`, memory is allocated without
  NUMA awareness, so bandwidth may be lower on multi-socket servers.
- `libhugetlbfs`, which detects the huge page size. Without `libhugetlbfs`,
  the huge page size is read from `/proc/meminfo`.

## Installing Dependencies

The following commands install the build dependencies, or only the runtime
libraries for a machine that runs a binary built elsewhere.

To build on Debian and Ubuntu:

```bash
apt-get install build-essential libhwloc-dev libnuma-dev libhugetlbfs-dev
```

To install only the runtime libraries on Debian and Ubuntu, as in the Docker
image:

```bash
apt-get install libgomp1 libhwloc15 libnuma1 libhugetlbfs-dev
```

To build on RHEL (Red Hat Enterprise Linux), CentOS, and Fedora:

```bash
yum install gcc make hwloc-devel numactl-devel libhugetlbfs-devel
```

To install only the runtime libraries on RHEL, CentOS, and Fedora:

```bash
yum install libgomp hwloc-libs numactl-libs libhugetlbfs
```

On macOS, install the LLVM `clang` from Homebrew, since Apple's `clang` has
no OpenMP, and point the build at it. There is no NUMA support on macOS:

```bash
brew install llvm hwloc
export CC="$(brew --prefix llvm)/bin/clang"
export LIBRARY_PATH="$(brew --prefix llvm)/lib"
```

On FreeBSD, install GNU `make` and `hwloc` 2 (`hwloc2`, not `hwloc` 1):

```bash
pkg install gmake hwloc2
```

CI ([`ci.yml`](../.github/workflows/ci.yml)) uses the macOS recipe above. On
Ubuntu, it installs `libhwloc-dev`, `libnuma-dev`, and `libhugetlbfs-dev`,
without `build-essential`:

```bash
sudo apt-get update && sudo apt-get install -y libhwloc-dev libnuma-dev libhugetlbfs-dev
```

On FreeBSD, it installs the following:

```bash
pkg install -y gmake hwloc2
```

## Build Targets

Build one of the following targets (with `gmake` on FreeBSD) in a clone of
the [`sc-membench` repository](https://github.com/SpareCores/sc-membench).
The table names the binary that each target builds, where there is one, and
says what the target does:

| Command | Builds | Description |
| --- | --- | --- |
| `make` | `membench` | Links `hwloc`, `libnuma`, and `libhugetlbfs` when detected |
| `make basic` | `membench-basic` | No optional library (caches from `/sys` or `sysctl`) |
| `make hwloc` | `membench-hwloc` | `hwloc` 2 only (portable cache detection) |
| `make numa` | `membench-numa` | `libnuma` only (Linux) |
| `make full` | `membench-full` | `hwloc` and `libnuma`, plus `libhugetlbfs` if detected (recommended for servers; `hwloc` only outside Linux) |
| `make all` | Every variant | Every variant the platform can build |
| `make test` | `membench` | Builds `membench` and runs a quick 30-second test (`./membench -v -t 30`) |
| `make install` | `membench` | Installs the binary and the man page under `/usr/local` |
| `make info` | None | Prints the detected configuration |
| `make clean` | None | Removes the built binaries |
| `make help` | None | Prints the build targets |

Set `CC` to use another compiler. The compiler flags are `-O3`, `-Wall`,
`-Wextra`, `-std=c11`, and `-fopenmp`, with `-march=x86-64` and
`-mtune=generic` on Linux x86_64 and `-mcpu=generic+crc` on aarch64 (arm64),
so that one binary runs on every CPU of the architecture.

## Recommended Build

For use on servers, build with all features. To get the source:

```bash
git clone https://github.com/SpareCores/sc-membench
cd sc-membench
```

To install the dependencies first, on Debian and Ubuntu:

```bash
sudo apt-get install build-essential libhugetlbfs-dev libhwloc-dev libnuma-dev
```

Or on RHEL and CentOS:

```bash
sudo yum install gcc make libhugetlbfs-devel hwloc-devel numactl-devel
```

To build with full features and run:

```bash
make full
./membench-full -v
```

See [Running Without Docker](./usage.md#running-without-docker) for how to
run the binary.

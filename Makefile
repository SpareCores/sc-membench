# sc-membench Makefile
#
# Portable build system for Linux, macOS, and BSD
# Uses OpenMP for parallel bandwidth measurement
#
# Build options:
#   make           - Auto-detect features and build universal binary
#   make basic     - Build minimal version (no optional dependencies)
#   make hwloc     - Build with hwloc (portable cache detection)
#   make numa      - Build with NUMA support (Linux only)
#   make full      - Build with all available features
#   make clean     - Remove built files
#   make test      - Quick test run

# =============================================================================
# Platform and Compiler Detection
# =============================================================================

# Detect OS
UNAME_S := $(shell uname -s)

# Auto-detect compiler: prefer gcc, fall back to clang, then cc
CC ?= $(shell command -v gcc 2>/dev/null || command -v clang 2>/dev/null || echo cc)

# Base flags (portable across gcc/clang)
CFLAGS_BASE = -O3 -Wall -Wextra -std=c11

# OpenMP flag (same for gcc and clang)
OPENMP_FLAG = -fopenmp

# =============================================================================
# Source Files and Targets
# =============================================================================

SRCS = $(wildcard *.c)
HDRS = $(wildcard *.h)
TARGET = membench
TARGET_BASIC = membench-basic
TARGET_HWLOC = membench-hwloc
TARGET_NUMA = membench-numa
TARGET_FULL = membench-full

# =============================================================================
# Platform-Specific Universal Optimization Flags
# =============================================================================

# Platform-specific adjustments with UNIVERSAL compatibility
ifeq ($(UNAME_S),Darwin)
    # macOS: packages typically in /opt/homebrew (ARM) or /usr/local (Intel)
    ARCH := $(shell uname -m)
    ifeq ($(ARCH),arm64)
        # ARM64 macOS: Use generic ARMv8-A (works on all Apple Silicon)
        CFLAGS_ARCH = -mcpu=generic
        CFLAGS_PATHS = -I/opt/homebrew/include
        LDFLAGS_PATHS = -L/opt/homebrew/lib
    else
        # x86_64 macOS: Use baseline x86-64 (works on all Intel Macs)
        CFLAGS_ARCH = -march=x86-64 -mtune=generic
        CFLAGS_PATHS = -I/usr/local/include
        LDFLAGS_PATHS = -L/usr/local/lib
    endif
    # macOS with clang needs libomp
    LDFLAGS_BASE = -lm
    # Check if using clang (needs -lomp for OpenMP)
    IS_CLANG := $(shell $(CC) --version 2>/dev/null | grep -q clang && echo yes)
    ifeq ($(IS_CLANG),yes)
        OPENMP_LIBS = -lomp
    else
        OPENMP_LIBS =
    endif
else ifeq ($(UNAME_S),FreeBSD)
    # FreeBSD: packages in /usr/local, use baseline x86-64
    CFLAGS_ARCH = -march=x86-64 -mtune=generic
    CFLAGS_PATHS = -I/usr/local/include
    LDFLAGS_PATHS = -L/usr/local/lib
    LDFLAGS_BASE = -lm
    OPENMP_LIBS =
else ifeq ($(UNAME_S),OpenBSD)
    CFLAGS_ARCH = -march=x86-64 -mtune=generic
    CFLAGS_PATHS = -I/usr/local/include
    LDFLAGS_PATHS = -L/usr/local/lib
    LDFLAGS_BASE = -lm
    OPENMP_LIBS =
else ifeq ($(UNAME_S),NetBSD)
    CFLAGS_ARCH = -march=x86-64 -mtune=generic
    CFLAGS_PATHS = -I/usr/local/include -I/usr/pkg/include
    LDFLAGS_PATHS = -L/usr/local/lib -L/usr/pkg/lib
    LDFLAGS_BASE = -lm
    OPENMP_LIBS =
else
    # Linux (default) - Use conservative, universally compatible flags
    ARCH := $(shell uname -m)
    ifeq ($(ARCH),aarch64)
        # ARM64: Use generic ARMv8-A with CRC (universally supported)
        # This works on all ARM64 CPUs from Cortex-A53 to Neoverse-V2
        CFLAGS_ARCH = -mcpu=generic+crc
    else ifeq ($(ARCH),x86_64)
        # x86_64: Use baseline x86-64 with SSE2 (universally supported since 2003)
        # This works on all x86_64 CPUs from Opteron/Pentium 4 to latest Xeon/EPYC
        CFLAGS_ARCH = -march=x86-64 -mtune=generic
    else
        # Other architectures: use generic optimization
        CFLAGS_ARCH = -mtune=generic
    endif
    # glibc hides POSIX/GNU APIs (sched_setaffinity, clock_gettime, getopt, ...) under -std=c11
    CFLAGS_OS = -D_GNU_SOURCE
    CFLAGS_PATHS =
    LDFLAGS_PATHS =
    LDFLAGS_BASE = -lm
    OPENMP_LIBS =
endif

CFLAGS = $(CFLAGS_BASE) $(CFLAGS_ARCH) $(CFLAGS_OS) $(CFLAGS_PATHS) $(OPENMP_FLAG)
LDFLAGS = $(OPENMP_FLAG) $(LDFLAGS_BASE) $(LDFLAGS_PATHS) $(OPENMP_LIBS)

# =============================================================================
# Library Detection
# =============================================================================

# Check for hwloc (cross-platform)
HAVE_HWLOC := $(shell pkg-config --exists hwloc 2>/dev/null && echo yes || \
                      (test -f /usr/include/hwloc.h && echo yes) || \
                      (test -f /usr/local/include/hwloc.h && echo yes) || \
                      (test -f /opt/homebrew/include/hwloc.h && echo yes))

# Check for libnuma (Linux only)
ifeq ($(UNAME_S),Linux)
    HAVE_NUMA := $(shell pkg-config --exists numa 2>/dev/null && echo yes || \
                         (test -f /usr/include/numa.h && echo yes))
else
    HAVE_NUMA := no
endif

# Check for libhugetlbfs (Linux only)
ifeq ($(UNAME_S),Linux)
    HAVE_HUGETLBFS := $(shell pkg-config --exists hugetlbfs 2>/dev/null && echo yes || \
                              (test -f /usr/include/hugetlbfs.h && echo yes))
else
    HAVE_HUGETLBFS := no
endif

# Auto-detect features and compile with all available
DETECTED_DEFS =
DETECTED_LIBS =

ifeq ($(HAVE_HUGETLBFS),yes)
    DETECTED_DEFS += -DHAVE_HUGETLBFS
    DETECTED_LIBS += -lhugetlbfs
endif

ifeq ($(HAVE_HWLOC),yes)
    DETECTED_DEFS += -DUSE_HWLOC
    DETECTED_LIBS += -lhwloc
endif

ifeq ($(HAVE_NUMA),yes)
    DETECTED_DEFS += -DUSE_NUMA
    DETECTED_LIBS += -lnuma
endif

# =============================================================================
# Build Targets
# =============================================================================

.PHONY: default all clean test basic hwloc numa full help info

# Default: auto-detect and build universal binary
default: $(TARGET)

$(TARGET): $(SRCS) $(HDRS)
	@echo "Building universal binary for $(UNAME_S) $(ARCH)..."
	@echo "  Compiler: $(CC)"
	@echo "  Optimization: $(CFLAGS_ARCH) (universal compatibility)"
	@echo "  OpenMP:   enabled"
	@echo "  hwloc:    $(HAVE_HWLOC)"
	@echo "  numa:     $(HAVE_NUMA)"
	@echo "  hugetlbfs: $(HAVE_HUGETLBFS)"
	$(CC) $(CFLAGS) $(DETECTED_DEFS) -o $@ $(SRCS) $(LDFLAGS) $(DETECTED_LIBS)

# Basic: minimal build, no optional dependencies
basic: $(TARGET_BASIC)

$(TARGET_BASIC): $(SRCS) $(HDRS)
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS)

# Build with hwloc support (portable cache/topology detection)
hwloc: $(TARGET_HWLOC)

$(TARGET_HWLOC): $(SRCS) $(HDRS)
ifeq ($(HAVE_HWLOC),yes)
	$(CC) $(CFLAGS) -DUSE_HWLOC -o $@ $(SRCS) $(LDFLAGS) -lhwloc
else
	@echo "Error: hwloc 2.x not found. Install with:"
	@echo "  Linux:  apt install libhwloc-dev  (or: yum install hwloc-devel)"
	@echo "  macOS:  brew install hwloc"
	@echo "  BSD:    pkg install hwloc2"
	@exit 1
endif

# Build with NUMA support (Linux only)
numa: $(TARGET_NUMA)

$(TARGET_NUMA): $(SRCS) $(HDRS)
ifeq ($(UNAME_S),Linux)
ifeq ($(HAVE_NUMA),yes)
	$(CC) $(CFLAGS) -DUSE_NUMA -o $@ $(SRCS) $(LDFLAGS) -lnuma
else
	@echo "Error: libnuma not found. Install with:"
	@echo "  apt install libnuma-dev  (or: yum install numactl-devel)"
	@exit 1
endif
else
	@echo "Error: NUMA support is only available on Linux"
	@exit 1
endif

# Build with all features (recommended for production Linux servers)
full: $(TARGET_FULL)

$(TARGET_FULL): $(SRCS) $(HDRS)
ifeq ($(UNAME_S),Linux)
	$(CC) $(CFLAGS) -DUSE_HWLOC -DUSE_NUMA $(if $(filter yes,$(HAVE_HUGETLBFS)),-DHAVE_HUGETLBFS) \
		-o $@ $(SRCS) $(LDFLAGS) -lhwloc -lnuma $(if $(filter yes,$(HAVE_HUGETLBFS)),-lhugetlbfs)
else
	@echo "Note: Building without NUMA (not available on $(UNAME_S))"
	$(CC) $(CFLAGS) -DUSE_HWLOC -o $@ $(SRCS) $(LDFLAGS) -lhwloc
endif

# Build all versions that can be built on this platform
all: $(TARGET) $(TARGET_BASIC)
ifeq ($(HAVE_HWLOC),yes)
	$(MAKE) hwloc
endif
ifeq ($(HAVE_NUMA),yes)
	$(MAKE) numa
endif
ifeq ($(UNAME_S),Linux)
ifeq ($(HAVE_HWLOC),yes)
ifeq ($(HAVE_NUMA),yes)
	$(MAKE) full
endif
endif
endif

# Quick test (30 seconds)
test: $(TARGET)
	./$(TARGET) -v -t 30

clean:
	rm -f $(TARGET) $(TARGET_BASIC) $(TARGET_HWLOC) $(TARGET_NUMA) $(TARGET_FULL)

# Install to /usr/local (override with: make install prefix=/usr)
prefix ?= /usr/local
mandir ?= $(prefix)/share/man

install: $(TARGET)
	install -d $(DESTDIR)$(prefix)/bin
	install -m 755 $(TARGET) $(DESTDIR)$(prefix)/bin/membench
	install -d $(DESTDIR)$(mandir)/man1
	install -m 644 membench.1 $(DESTDIR)$(mandir)/man1/membench.1

# Show detected configuration
info:
	@echo "Platform Detection:"
	@echo "  OS:        $(UNAME_S)"
	@echo "  Arch:      $(ARCH)"
	@echo "  Compiler:  $(CC)"
	@echo "  CFLAGS:    $(CFLAGS)"
	@echo "  LDFLAGS:   $(LDFLAGS)"
	@echo ""
	@echo "Library Detection:"
	@echo "  hwloc:     $(HAVE_HWLOC)"
	@echo "  numa:      $(HAVE_NUMA)"
	@echo "  hugetlbfs: $(HAVE_HUGETLBFS)"
	@echo ""
	@echo "Universal Optimization:"
ifeq ($(ARCH),aarch64)
	@echo "  ARM64:     -mcpu=generic+crc (works on all ARM64 CPUs)"
else ifeq ($(ARCH),x86_64)
	@echo "  x86_64:    -march=x86-64 (works on all x86_64 CPUs since 2003)"
else
	@echo "  Other:     -mtune=generic"
endif

# Help target
help:
	@echo "sc-membench - Universal Memory Benchmark (OpenMP)"
	@echo ""
	@echo "Build targets:"
	@echo "  make          - Auto-detect features and build universal binary"
	@echo "  make basic    - Minimal build (no optional dependencies)"
	@echo "  make hwloc    - With hwloc (portable cache detection)"
	@echo "  make numa     - With NUMA support (Linux only)"
	@echo "  make full     - With all features (hwloc + numa, Linux recommended)"
	@echo "  make all      - Build all available versions"
	@echo "  make info     - Show detected platform and libraries"
	@echo ""
	@echo "Universal Compatibility:"
	@echo "  This build system uses conservative optimization flags that work"
	@echo "  on ALL CPUs of the target architecture:"
	@echo "    - ARM64: -mcpu=generic+crc (Cortex-A53 to Neoverse-V2)"
	@echo "    - x86_64: -march=x86-64 (Opteron/P4 to latest Xeon/EPYC)"
	@echo "  No illegal instruction errors, works in any Docker container."
	@echo ""
	@echo "OpenMP thread control (environment variables):"
	@echo "  OMP_PROC_BIND=spread    Distribute threads across NUMA nodes"
	@echo "  OMP_PLACES=cores        One thread per physical core"
	@echo "  OMP_NUM_THREADS=N       Override thread count"
	@echo ""
	@echo "Optional dependencies:"
	@echo "  hwloc 2:   Portable cache/topology detection (requires hwloc 2.x)"
	@echo "             Linux: apt install libhwloc-dev"
	@echo "             macOS: brew install hwloc libomp"
	@echo "             BSD:   pkg install hwloc2"
	@echo ""
	@echo "  numa:      NUMA-aware memory allocation (Linux only)"
	@echo "             apt install libnuma-dev"
	@echo ""
	@echo "  hugetlbfs: Better huge page detection (Linux only)"
	@echo "             apt install libhugetlbfs-dev"
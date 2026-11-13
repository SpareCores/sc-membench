# References

Terms specific to this image, alphabetized. Common terms (e.g. CV, NUMA,
OpenMP, TLB) are defined in the
[glossary](https://docs.sparecores.com/glossary) of the Spare Cores docs,
which also lists the terms each image uses.

- **Bandwidth Score**: One of the three membench scores: the mean of the
  peak read, write, and copy bandwidths / 1,000, in units of 1,000 MiB/s,
  which `membench` labels "GB/s". A higher score is better.
- **Combined Score**: One of the three membench scores:
  100 × √(Bandwidth Score × Latency Score), with no unit. As a geometric
  mean, it lets neither bandwidth nor latency dominate.
- **`hwloc`** (Portable Hardware Locality): A library and set of tools that
  report the hardware topology of a host in the same way on every operating
  system. The topology covers NUMA nodes, sockets, cores, hardware threads,
  and the size and sharing of each cache level. Programs use it to place
  threads and to size their work for the caches.
  [hwloc](https://www.open-mpi.org/projects/hwloc/)
- **`lat_mem_rd`**: The memory latency tool of LMbench. It follows a chain of
  pointers through arrays of increasing size and reports, for each size, the
  time of one load in nanoseconds, which shows the latency of each cache
  level and of main memory.
  [`lat_mem_rd` manual](https://github.com/intel/lmbench/blob/master/doc/lat_mem_rd.8)
- **Latency Score**: One of the three membench scores: 1,000 / the latency
  in ns at the largest buffer size tested, that is, dependent loads per
  microsecond. A higher score is better.
- **`libhugetlbfs`**: A Linux library that helps programs use huge pages, for
  example by backing their heap with them, and that reports the huge page
  sizes the system supports.
  [GitHub](https://github.com/libhugetlbfs/libhugetlbfs)
- **`libnuma`**: The Linux library behind the `numactl` command, with which a
  program sets its own NUMA policy. Its functions report the NUMA node of
  each CPU and allocate memory on a chosen node, such as
  `numa_alloc_onnode`.
  [man page](https://man7.org/linux/man-pages/man3/numa.3.html)
- **`MADV_HUGEPAGE`**: A flag of the Linux `madvise` system call that asks the
  kernel to back a memory range with Transparent Huge Pages. It is a request,
  not a guarantee, and needs no huge pages reserved in advance.
  [man page](https://man7.org/linux/man-pages/man2/madvise.2.html)
- **`MAP_HUGETLB`**: A flag of the Linux `mmap` system call that allocates a
  mapping from the pool of explicitly reserved huge pages. The call fails
  when the pool has too few free pages, so programs usually fall back to
  another allocation.
  [man page](https://man7.org/linux/man-pages/man2/mmap.2.html)
- **`membench`**: A memory benchmark tool written in C by Spare Cores, also
  called `sc-membench`. It measures read, write, and copy bandwidth with one
  buffer per OpenMP thread, and memory latency with a pointer chase, over a
  series of buffer sizes derived from the CPU's cache sizes.
  [GitHub](https://github.com/SpareCores/sc-membench)
- **membench memory guard**, **memory guard**: The rule with which `membench`
  limits its memory use: it halves the thread count of a bandwidth
  measurement until the buffers of all threads fit in a quarter of the
  system's RAM.
- **membench scores**: The three numbers that `membench` prints in its
  human-readable summary: the Bandwidth Score, the Latency Score, and the
  Combined Score.
- **membench size sweep**, **size sweep**, **sweep**: The series of
  per-thread buffer sizes that `membench` tests in one run, in ascending
  order. By default, it holds up to 10 sizes placed around the L1d, L2, and
  L3 cache sizes, from half the L1d cache to four times the L3 cache.
- **`proc_bind(spread)`**: An OpenMP clause that sets the thread affinity
  policy of a parallel region. With `spread`, the runtime distributes the
  threads as evenly as possible over the available places, such as cores or
  sockets, and keeps each thread on its place.
  [OpenMP specification](https://www.openmp.org/spec-html/5.0/openmpsu36.html)
- **`ram_bench`**: A small C++ benchmark by Emil Ernerfeldt that measures the
  cost of random memory access by traversing linked lists of growing size
  whose nodes are in random order.
  [GitHub](https://github.com/emilk/ram_bench)

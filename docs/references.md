# References

Terms specific to this image, alphabetized. Common terms (e.g. CV, NUMA,
OpenMP, TLB) are defined in the
[glossary](https://docs.sparecores.com/glossary) of the Spare Cores docs,
which also lists the terms each image uses.

- **`membench`**: A memory benchmark tool written in C by Spare Cores, also
  called sc-membench. It measures read, write, and copy bandwidth with one
  buffer per OpenMP thread, and memory latency with a pointer chase, over a
  series of buffer sizes derived from the CPU's cache sizes.
  [GitHub](https://github.com/SpareCores/sc-membench)
- **membench memory guard**, **memory guard**: The rule with which `membench`
  limits its memory use: it halves the thread count of a bandwidth
  measurement until the buffers of all threads fit in a quarter of the
  system's RAM.
- **membench scores**, **Bandwidth Score**, **Latency Score**,
  **Combined Score**: The three numbers that `membench` prints in its
  human-readable summary. The Bandwidth Score is the mean of the peak read,
  write, and copy bandwidths divided by 1,000, the Latency Score is 1,000
  divided by the latency at the largest buffer size, and the Combined Score
  is the square root of their product times 100.
- **membench size sweep**, **size sweep**, **sweep**: The series of
  per-thread buffer sizes that `membench` tests in one run, in ascending
  order. By default, it holds up to 10 sizes placed around the L1d, L2, and
  L3 cache sizes, from half the L1d cache to four times the L3 cache.
- **`ram_bench`**: A small C++ benchmark by Emil Ernerfeldt that measures the
  cost of random memory access by traversing linked lists of growing size
  whose nodes are in random order.
  [GitHub](https://github.com/emilk/ram_bench)

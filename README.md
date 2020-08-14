# Substring Search Benchmark

The purpose of this repo is to demonstrate the importance of low-level optimizations in mission-critical applications.
This implementation is specific to AVX2 substed of x86 instruction set, but similar results should be expected on ARM hardware.

---

There is a lot of variance between different runs.<br/>
Following numbers were measured on an 8-core 2019 MacBook Pro 16".<br/>
The haystack size was `= 512 MB`.<br/>
The needle size was `5-8 Bytes`.<br/>

| Benchmark                       | Bytes/Sec |
| :------------------------------ | :-------: |
| Python                          |  20 MB/s  |
| `av::naive_t` in C++            | ~1,5 GB/s |
| `av::prefixed_t` in C++         | ~2,5 GB/s |
| `av::prefixed_avx2_t` in C++    |  ~8 GB/s  |
| `av::hybrid_avx2_t` in C++      |  ~9 GB/s  |
| `av::speculative_avx2_t` in C++ | ~12 GB/s  |

---

If you are interested in high-performance software and algorithm design - check out [Unum](https://unum.xyz).
The `Unum.DB` is our in-house database developed with similar tricks and it's orders of magniture faster than alternatives!

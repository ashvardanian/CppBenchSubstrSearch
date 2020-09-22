# Substring Search Benchmark

The purpose of this repo is to demonstrate the importance of low-level optimizations in mission-critical applications.
This implementation is specific to AVX2 substed of x86 instruction set, but similar results should be expected on ARM hardware.

---

There is a lot of variance between different runs.<br/>
Following numbers were measured on an 8-core 2019 MacBook Pro 16".<br/>
The haystack size was `= 512 MB`.<br/>
The needle size was `10 Bytes`.<br/>

| Benchmark                       | Rich Alphabet<br/>(25 chars) | Poor Alphabet<br/>(57 chars) |
| :------------------------------ | :--------------------------: | :--------------------------: |
| Python                          |           16 MB/s            |           15 MB/s            |
| `std::string::find` in C++      |           1,9 GB/s           |           1,3 GB/s           |
| `av::naive_t` in C++            |           1,4 GB/s           |           1,1 GB/s           |
| `av::prefixed_t` in C++         |           3,5 GB/s           |           3,5 GB/s           |
| `av::prefixed_avx2_t` in C++    |           9,8 GB/s           |           9,6 GB/s           |
| `av::hybrid_avx2_t` in C++      |           9,7 GB/s           |           9,6 GB/s           |
| `av::speculative_avx2_t` in C++ |          12,2 GB/s           |          12,1 GB/s           |

---

If you are interested in high-performance software and algorithm design - check out [Unum](https://unum.xyz).
The `Unum.DB` is our in-house database developed with similar tricks and it's orders of magniture faster than alternatives!

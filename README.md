# Substring Search Benchmark

The purpose of this repo is to demonstrate the importance of low-level optimizations in mission-critical applications.
This implementation is specific to AVX2 substed of x86 instruction set, but similar results should be expected on ARM hardware.

---

There is a lot of variance between different runs.<br/>
The haystack size was `= 512M Bytes`.<br/>
The needle size was `10 Bytes`.<br/>

| Benchmark                         | Scan Performance |
| :-------------------------------- | :--------------: |
| Python                            |    < 20 MB/s     |
| `std::string::find` in C++        |     1,2 GB/s     |
|                                   |                  |
| `av::naive_t` in C++              |     1,0 GB/s     |
| `av::prefixed_t` in C++           |     3,3 GB/s     |
|                                   |                  |
| `av::prefixed_avx2_t` in C++      |     8,5 GB/s     |
| `av::hybrid_avx2_t` in C++        |     9,1 GB/s     |
| `av::speculative_avx2_t` in C++   |    12,0 GB/s     |
| `av::speculative_avx512_t` in C++ |    12,1 GB/s     |

---

CPUs used:
* MacBook Pro 2019 16": [Intel Core i9-9880H](https://ark.intel.com/content/www/us/en/ark/products/192987/intel-core-i9-9880h-processor-16m-cache-up-to-4-80-ghz.html).
  * TDP: 45W / 8 cores = 5.6 W/core.
  * 2019 model. No AVX-512 support.
* Intel Server: [Intel Xeon Gold 6152](https://ark.intel.com/content/www/us/en/ark/products/120491/intel-xeon-gold-6152-processor-30-25m-cache-2-10-ghz.html).
  * TDP: 140W / 22 cores = 6.5 W/core.
  * 2017 model. Has AVX-512 support.

---

If you are interested in high-performance software and algorithm design - check out [Unum](https://unum.xyz).
The `Unum.DB` is our in-house database developed with similar tricks and it's orders of magniture faster than the alternatives in both read and write operations!

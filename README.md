# Substring Search Benchmark

The purpose of this repo is to demonstrate the importance of low-level optimizations in mission-critical applications.
This implementation is specific to AVX2 substed of x86 instruction set, but similar results should be expected on ARM hardware.

## Algorithms

There is a lot of variance between different runs.<br/>
The haystack size was `= 512M Bytes`.<br/>
The needle size was `10 Bytes`.<br/>

| Benchmark                         |   IoT    |  Laptop   |  Server   |
| :-------------------------------- | :------: | :-------: | :-------: |
| Python                            |  4 MB/s  |  14 MB/s  |  11 MB/s  |
| `std::string::find` in C++        | 560 MB/s | 1,2 GB/s  | 1,3 GB/s  |
|                                   |          |
| `av::naive_t` in C++              | 520 MB/s | 1,0 GB/s  | 900 MB/s  |
| `av::prefixed_t` in C++           | 2,2 GB/s | 3,3 GB/s  | 3,5 GB/s  |
|                                   |          |
| `av::prefixed_avx2_t` in C++      |          | 8,5 GB/s  | 10,5 GB/s |
| `av::hybrid_avx2_t` in C++        |          | 9,1 GB/s  | 9,1 GB/s  |
| `av::speculative_avx2_t` in C++   |          | 12,0 GB/s | 9,9 GB/s  |
| `av::speculative_avx512_t` in C++ |          |           | 9,9 GB/s  |
| `av::speculative_neon_t` in C++   |          |           |           |

Devices used:

* IoT:
  * Model: Nvidia Jetson AGX:
  * CPU: 8-core 64-bit Arm v8.2.
  * CPU TDP: 30W / 8 cores = 3.7 W/core.
  * Year: 2018.
  * ARMv8 features: `fp asimd evtstrm aes pmull sha1 sha2 crc32 atomics fphp asimdhp`.

* Laptop:
  * Model: Apple MacBook Pro 16".
  * CPU: 8-core [Intel Core i9-9880H](https://ark.intel.com/content/www/us/en/ark/products/192987/intel-core-i9-9880h-processor-16m-cache-up-to-4-80-ghz.html).
  * CPU TDP: 45W / 8 cores = 5.6 W/core.
  * Year: 2019.
  * No AVX-512 support.

* Server:
  * CPU: 2x 22-core [Intel Xeon Gold 6152](https://ark.intel.com/content/www/us/en/ark/products/120491/intel-xeon-gold-6152-processor-30-25m-cache-2-10-ghz.html).
  * CPU TDP: 140W / 22 cores = 6.5 W/core.
  * Year: 2017.
  * Has AVX-512 support.

## Analysis

Using Intel Advisor one can see, that the `av::speculative_avx2_t` reaches the hardware limit - it's mostly memory-bound, but may also be compute-bound.

![Intel Advisor results](results/intel_advisor.png)

---

If you are interested in high-performance software and algorithm design - check out [Unum](https://unum.xyz).
The `Unum.DB` is our in-house database developed with similar tricks and it's orders of magniture faster than the alternatives in both read and write operations!

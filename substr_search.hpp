#pragma once
#include <stdint.h> // `uint8_t`
#include <stddef.h> // `size_t`
#include <omp.h>    // pragmas
#ifdef __AVX2__
#include <immintrin.h> // `__m256i`
#endif
#ifdef __ARM_NEON
#include <arm_neon.h>
#endif
#include <limits>      // `numeric_limits`
#include <string_view> // `basic_string_view`
#include "substr_seach_macros.hpp"

namespace av {

    static constexpr size_t not_found_k = std::numeric_limits<size_t>::max();

    struct span_t {
        uint8_t *data = nullptr;
        size_t len = 0;

        inline span_t after_n(size_t offset) const noexcept {
            return (offset < len) ? span_t {data + offset, len - offset} : span_t {};
        }
    };

    /**
     * \brief This is a faster alternative to `strncmp(a, b, len) == 0`.
     */
    template <typename int_at>
    inline bool are_equal(int_at const *a, int_at const *b, size_t len) noexcept {
        int_at const *const a_end = a + len;
        for (; a != a_end && *a == *b; a++, b++)
            ;
        return a_end == a;
    }

    struct stl_t {

        size_t next_offset(span_t haystack, span_t needle) noexcept {
            using str_view_t = std::basic_string_view<uint8_t>;
            str_view_t h_stl {haystack.data, haystack.len};
            str_view_t n_stl {needle.data, needle.len};
            size_t off = h_stl.find(n_stl);
            return off == str_view_t::npos ? not_found_k : off;
        }
    };

    /**
     * \brief A naive subtring matching algorithm with O(|haystack|*|needle|) comparisons.
     * Matching performance fluctuates between 200 MB/s and 2 GB/s.
     */
    struct naive_t {

        size_t next_offset(span_t haystack, span_t needle) noexcept {

            if (haystack.len < needle.len)
                return not_found_k;

            for (size_t off = 0; off <= haystack.len - needle.len; off++) {
                if (are_equal(haystack.data + off, needle.data, needle.len))
                    return off;
            }

            return not_found_k;
        }
    };

    /**
     * \brief Modified version inspired by Rabin-Karp algorithm.
     * Matching performance fluctuates between 1 GB/s and 3,5 GB/s.
     *
     * Similar to Rabin-Karp Algorithm, instead of comparing variable length
     * strings - we can compare some fixed size fingerprints, which can make
     * the number of nested loops smaller. But preprocessing text to generate
     * hashes is very expensive.
     * Instead - we compare the first 4 bytes of the `needle` to every 4 byte
     * substring in the `haystack`. If those match - compare the rest.
     */
    struct prefixed_t {

        size_t next_offset(span_t haystack, span_t needle) noexcept {

            if (needle.len < 5)
                return naive_t {}.next_offset(haystack, needle);

            // Precomputed constants.
            uint8_t const *h_ptr = haystack.data;
            uint8_t const *const h_end = haystack.data + haystack.len - needle.len;
            size_t const n_suffix_len = needle.len - 4;
            uint32_t const n_prefix = *reinterpret_cast<uint32_t const *>(needle.data);
            uint8_t const *n_suffix_ptr = needle.data + 4;

            for (; h_ptr <= h_end; h_ptr++) {
                if (n_prefix == *reinterpret_cast<uint32_t const *>(h_ptr))
                    if (are_equal(h_ptr + 4, n_suffix_ptr, n_suffix_len))
                        return h_ptr - haystack.data;
            }

            return not_found_k;
        }
    };

    struct prefixed_autovec_t {

        size_t next_offset(span_t haystack, span_t needle) noexcept {

            if (needle.len < 5)
                return naive_t {}.next_offset(haystack, needle);

            uint8_t const *h_ptr = haystack.data;
            uint8_t const *const h_end = haystack.data + haystack.len - needle.len;
            uint32_t const n_prefix = *reinterpret_cast<uint32_t const *>(needle.data);

            for (; (h_ptr + 32) <= h_end; h_ptr += 32) {

                int count_matches = 0;

#if compiler_is_clang_m
#pragma clang loop vectorize(enable)
                for (size_t i = 0; i < 32; i++)
                    count_matches += (n_prefix == *reinterpret_cast<uint32_t const *>(h_ptr + i));
#elif compiler_is_intel_m
#pragma vector always
#pragma ivdep
                for (size_t i = 0; i < 32; i++)
                    count_matches += (n_prefix == *reinterpret_cast<uint32_t const *>(h_ptr + i));
#elif compiler_is_gcc_m
#pragma ivdep
                for (size_t i = 0; i < 32; i++)
                    count_matches += (n_prefix == *reinterpret_cast<uint32_t const *>(h_ptr + i));
#else
#pragma omp for simd reduction(+ : count_matches)
                for (size_t i = 0; i < 32; i++)
                    count_matches += (n_prefix == *reinterpret_cast<uint32_t const *>(h_ptr + i));
#endif

                if (count_matches) {
                    for (size_t i = 0; i < 32; i++) {
                        if (are_equal(h_ptr + i, needle.data, needle.len))
                            return i + (h_ptr - haystack.data);
                    }
                }
            }

            // Don't forget the last (up to 35) characters.
            size_t last_match = prefixed_t {}.next_offset(haystack.after_n(h_ptr - haystack.data), needle);
            return (last_match != not_found_k) ? last_match + (h_ptr - haystack.data) : not_found_k;
        }
    };

#ifdef __AVX2__

    /**
     * \brief A SIMD vectorized version for AVX2 instruction set.
     * Matching performance is ~ 9 GB/s.
     *
     * This version processes 32 `haystack` substrings per iteration,
     * so the number of instructions is only:
     *  + 4 loads
     *  + 4 comparisons
     *  + 3 bitwise ORs
     *  + 1 masking
     * for every 32 consecutive substrings.
     */
    struct prefixed_avx2_t {

        size_t next_offset(span_t haystack, span_t needle) noexcept {

            if (needle.len < 5)
                return naive_t {}.next_offset(haystack, needle);

            uint8_t const *const h_end = haystack.data + haystack.len - needle.len;
            __m256i const n_prefix = _mm256_set1_epi32(*(uint32_t const *)(needle.data));

            uint8_t const *h_ptr = haystack.data;
            for (; (h_ptr + 32) <= h_end; h_ptr += 32) {

                __m256i h0 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr)), n_prefix);
                __m256i h1 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 1)), n_prefix);
                __m256i h2 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 2)), n_prefix);
                __m256i h3 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 3)), n_prefix);
                __m256i h_any = _mm256_or_si256(_mm256_or_si256(h0, h1), _mm256_or_si256(h2, h3));
                int mask = _mm256_movemask_epi8(h_any);

                if (mask) {
                    for (size_t i = 0; i < 32; i++) {
                        if (are_equal(h_ptr + i, needle.data, needle.len))
                            return i + (h_ptr - haystack.data);
                    }
                }
            }

            // Don't forget the last (up to 35) characters.
            size_t last_match = prefixed_t {}.next_offset(haystack.after_n(h_ptr - haystack.data), needle);
            return (last_match != not_found_k) ? last_match + (h_ptr - haystack.data) : not_found_k;
        }
    };

    /**
     * \brief Speculative SIMD version for AVX2 instruction set.
     * Matching performance is ~ 12 GB/s.
     *
     * Up to 40% of performance in modern CPUs comes from speculative
     * out-of-order execution. The `prefixed_avx2_t` version has
     * 4 explicit local memory  barries: 3 ORs and 1 IF branch.
     * This has only 1 IF branch in the main loop.
     */
    struct speculative_avx2_t {

        size_t next_offset(span_t haystack, span_t needle) noexcept {

            if (needle.len < 5)
                return naive_t {}.next_offset(haystack, needle);

            // Precomputed constants.
            uint8_t const *const h_end = haystack.data + haystack.len - needle.len;
            __m256i const n_prefix = _mm256_set1_epi32(*(uint32_t const *)(needle.data));

            // Top level for-loop changes dramatically.
            // In sequentail computing model for 32 offsets we would do:
            //  + 32 comparions.
            //  + 32 branches.
            // In vectorized computations models:
            //  + 4 vectorized comparisons.
            //  + 4 movemasks.
            //  + 3 bitwise ANDs.
            //  + 1 heavy (but very unlikely) branch.
            uint8_t const *h_ptr = haystack.data;
            for (; (h_ptr + 32) <= h_end; h_ptr += 32) {

                __m256i h0_prefixes = _mm256_loadu_si256((__m256i const *)(h_ptr));
                int masks0 = _mm256_movemask_epi8(_mm256_cmpeq_epi32(h0_prefixes, n_prefix));
                __m256i h1_prefixes = _mm256_loadu_si256((__m256i const *)(h_ptr + 1));
                int masks1 = _mm256_movemask_epi8(_mm256_cmpeq_epi32(h1_prefixes, n_prefix));
                __m256i h2_prefixes = _mm256_loadu_si256((__m256i const *)(h_ptr + 2));
                int masks2 = _mm256_movemask_epi8(_mm256_cmpeq_epi32(h2_prefixes, n_prefix));
                __m256i h3_prefixes = _mm256_loadu_si256((__m256i const *)(h_ptr + 3));
                int masks3 = _mm256_movemask_epi8(_mm256_cmpeq_epi32(h3_prefixes, n_prefix));

                if (masks0 | masks1 | masks2 | masks3) {
                    for (size_t i = 0; i < 32; i++) {
                        if (are_equal(h_ptr + i, needle.data, needle.len))
                            return i + (h_ptr - haystack.data);
                    }
                }
            }

            // Don't forget the last (up to 35) characters.
            size_t last_match = prefixed_t {}.next_offset(haystack.after_n(h_ptr - haystack.data), needle);
            return (last_match != not_found_k) ? last_match + (h_ptr - haystack.data) : not_found_k;
        }
    };

    /**
     * \brief A hybrid of `prefixed_avx2_t` and `speculative_avx2_t`.
     * It demonstrates the current inability of scheduler to optimize
     * the execution flow better, than a human.
     */
    struct hybrid_avx2_t {

        size_t next_offset(span_t haystack, span_t needle) noexcept {

            if (needle.len < 5)
                return naive_t {}.next_offset(haystack, needle);

            uint8_t const *const h_end = haystack.data + haystack.len - needle.len;
            __m256i const n_prefix = _mm256_set1_epi32(*(uint32_t const *)(needle.data));

            uint8_t const *h_ptr = haystack.data;
            for (; (h_ptr + 64) <= h_end; h_ptr += 64) {

                __m256i h0 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr)), n_prefix);
                __m256i h1 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 1)), n_prefix);
                __m256i h2 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 2)), n_prefix);
                __m256i h3 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 3)), n_prefix);
                int mask03 = _mm256_movemask_epi8(_mm256_or_si256(_mm256_or_si256(h0, h1), _mm256_or_si256(h2, h3)));

                __m256i h4 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 32)), n_prefix);
                __m256i h5 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 33)), n_prefix);
                __m256i h6 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 34)), n_prefix);
                __m256i h7 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 35)), n_prefix);
                int mask47 = _mm256_movemask_epi8(_mm256_or_si256(_mm256_or_si256(h4, h5), _mm256_or_si256(h6, h7)));

                if (mask03 | mask47) {
                    for (size_t i = 0; i < 64; i++) {
                        if (are_equal(h_ptr + i, needle.data, needle.len))
                            return i + (h_ptr - haystack.data);
                    }
                }
            }

            // Don't forget the last (up to 67) characters.
            size_t last_match = prefixed_t {}.next_offset(haystack.after_n(h_ptr - haystack.data), needle);
            return (last_match != not_found_k) ? last_match + (h_ptr - haystack.data) : not_found_k;
        }
    };

#endif

#ifdef __AVX512F__

    struct speculative_avx512_t {

        size_t next_offset(span_t haystack, span_t needle) noexcept {

            if (needle.len < 5)
                return naive_t {}.next_offset(haystack, needle);

            // Precomputed constants.
            uint8_t const *const h_end = haystack.data + haystack.len - needle.len;
            __m512i const n_prefix = _mm512_set1_epi32(*(uint32_t const *)(needle.data));

            uint8_t const *h_ptr = haystack.data;
            for (; (h_ptr + 64) <= h_end; h_ptr += 64) {

                __m512i h0_prefixes = _mm512_loadu_si512((__m512i const *)(h_ptr));
                int masks0 = _mm512_cmpeq_epi32_mask(h0_prefixes, n_prefix);
                __m512i h1_prefixes = _mm512_loadu_si512((__m512i const *)(h_ptr + 1));
                int masks1 = _mm512_cmpeq_epi32_mask(h1_prefixes, n_prefix);
                __m512i h2_prefixes = _mm512_loadu_si512((__m512i const *)(h_ptr + 2));
                int masks2 = _mm512_cmpeq_epi32_mask(h2_prefixes, n_prefix);
                __m512i h3_prefixes = _mm512_loadu_si512((__m512i const *)(h_ptr + 3));
                int masks3 = _mm512_cmpeq_epi32_mask(h3_prefixes, n_prefix);

                if (masks0 | masks1 | masks2 | masks3) {
                    for (size_t i = 0; i < 64; i++) {
                        if (are_equal(h_ptr + i, needle.data, needle.len))
                            return i + (h_ptr - haystack.data);
                    }
                }
            }

            // Don't forget the last (up to 64+3=67) characters.
            size_t last_match = prefixed_t {}.next_offset(haystack.after_n(h_ptr - haystack.data), needle);
            return (last_match != not_found_k) ? last_match + (h_ptr - haystack.data) : not_found_k;
        }
    };

#endif

#ifdef __ARM_NEON
    /**
     *  \brief 128-bit implementation for ARM Neon.
     *
     *  https://developer.arm.com/architectures/instruction-sets/simd-isas/neon/
     *  https://developer.arm.com/documentation/dui0473/m/neon-programming/neon-data-types
     *  https://developer.arm.com/documentation/dui0473/m/neon-programming/neon-vectors
     *  https://blog.cloudflare.com/neon-is-the-new-black/
     */
    struct speculative_neon_t {

        size_t next_offset(span_t haystack, span_t needle) noexcept {

            if (needle.len < 5)
                return naive_t {}.next_offset(haystack, needle);

            // Precomputed constants.
            uint8_t const *const h_end = haystack.data + haystack.len - needle.len;
            uint32x4_t const n_prefix = vld1q_dup_u32((uint32_t const *)(needle.data));

            uint8_t const *h_ptr = haystack.data;
            for (; (h_ptr + 16) <= h_end; h_ptr += 16) {

                uint32x4_t masks0 = vceqq_u32(vld1q_u32((uint32_t const *)(h_ptr)), n_prefix);
                uint32x4_t masks1 = vceqq_u32(vld1q_u32((uint32_t const *)(h_ptr + 1)), n_prefix);
                uint32x4_t masks2 = vceqq_u32(vld1q_u32((uint32_t const *)(h_ptr + 2)), n_prefix);
                uint32x4_t masks3 = vceqq_u32(vld1q_u32((uint32_t const *)(h_ptr + 3)), n_prefix);

                // Extracting matches from masks:
                // vmaxvq_u32 (only a64)
                // vgetq_lane_u32 (all)
                // vorrq_u32 (all)
                uint32x4_t masks = vorrq_u32(vorrq_u32(masks0, masks1), vorrq_u32(masks2, masks3));
                uint64x2_t masks64x2 = vreinterpretq_u64_u32(masks);
                bool has_match = vgetq_lane_u64(masks64x2, 0) | vgetq_lane_u64(masks64x2, 1);

                if (has_match) {
                    for (size_t i = 0; i < 16; i++) {
                        if (are_equal(h_ptr + i, needle.data, needle.len))
                            return i + (h_ptr - haystack.data);
                    }
                }
            }

            // Don't forget the last (up to 16+3=19) characters.
            size_t last_match = prefixed_t {}.next_offset(haystack.after_n(h_ptr - haystack.data), needle);
            return (last_match != not_found_k) ? last_match + (h_ptr - haystack.data) : not_found_k;
        }
    };

#endif

    /**
     * \return Total number of matches.
     */
    template <typename engine_at, typename callback_at>
    size_t find_all(span_t haystack, span_t needle, engine_at &&engine, callback_at &&callback) {

        size_t last_match = 0;
        size_t next_offset = 0;
        size_t count_matches = 0;
        for (; (last_match = engine.next_offset(haystack.after_n(next_offset), needle)) != not_found_k;
             count_matches++, next_offset = last_match + 1)
            callback(last_match);

        return count_matches;
    }

} // namespace av
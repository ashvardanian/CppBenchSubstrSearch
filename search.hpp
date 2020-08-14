#pragma once
#include <stdint.h>    // `uint8_t`
#include <immintrin.h> // `__m256i`
#include <limits>      // `numeric_limits`

namespace av {

    static constexpr size_t not_found_k = std::numeric_limits<size_t>::max();

    struct span_t {
        uint8_t *data = nullptr;
        size_t len = 0;

        inline span_t after_n(size_t offset) const {
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
            uint32_t const n_prefix = *reinterpret_cast<uint32_t const *>(needle.data);
            __m256i const n_prefix_x8 = _mm256_set1_epi32(n_prefix);

            uint8_t const *h_ptr = haystack.data;
            for (; (h_ptr + 32) <= h_end; h_ptr += 32) {

                __m256i h0 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr)), n_prefix_x8);
                __m256i h1 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 1)), n_prefix_x8);
                __m256i h2 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 2)), n_prefix_x8);
                __m256i h3 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 3)), n_prefix_x8);
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
            uint32_t const n_prefix = *reinterpret_cast<uint32_t const *>(needle.data);
            __m256i const n_prefix_x8 = _mm256_set1_epi32(n_prefix);

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

                __m256i h0_prefixes_x8 = _mm256_loadu_si256((__m256i const *)(h_ptr));
                int masks0 = _mm256_movemask_epi8(_mm256_cmpeq_epi32(h0_prefixes_x8, n_prefix_x8));
                __m256i h1_prefixes_x8 = _mm256_loadu_si256((__m256i const *)(h_ptr + 1));
                int masks1 = _mm256_movemask_epi8(_mm256_cmpeq_epi32(h1_prefixes_x8, n_prefix_x8));
                __m256i h2_prefixes_x8 = _mm256_loadu_si256((__m256i const *)(h_ptr + 2));
                int masks2 = _mm256_movemask_epi8(_mm256_cmpeq_epi32(h2_prefixes_x8, n_prefix_x8));
                __m256i h3_prefixes_x8 = _mm256_loadu_si256((__m256i const *)(h_ptr + 3));
                int masks3 = _mm256_movemask_epi8(_mm256_cmpeq_epi32(h3_prefixes_x8, n_prefix_x8));

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
            uint32_t const n_prefix = *reinterpret_cast<uint32_t const *>(needle.data);
            __m256i const n_prefix_x8 = _mm256_set1_epi32(n_prefix);

            uint8_t const *h_ptr = haystack.data;
            for (; (h_ptr + 64) <= h_end; h_ptr += 64) {

                __m256i h0 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr)), n_prefix_x8);
                __m256i h1 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 1)), n_prefix_x8);
                __m256i h2 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 2)), n_prefix_x8);
                __m256i h3 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 3)), n_prefix_x8);
                int mask03 = _mm256_movemask_epi8(_mm256_or_si256(_mm256_or_si256(h0, h1), _mm256_or_si256(h2, h3)));

                __m256i h4 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 32)), n_prefix_x8);
                __m256i h5 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 33)), n_prefix_x8);
                __m256i h6 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 34)), n_prefix_x8);
                __m256i h7 = _mm256_cmpeq_epi32(_mm256_loadu_si256((__m256i const *)(h_ptr + 35)), n_prefix_x8);
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

    /**
     * \return Total number of matches.
     */
    template <typename engine_at, typename callback_at>
    size_t enumerate_matches(span_t haystack, span_t needle, engine_at &&engine, callback_at &&callback) {

        size_t last_match = 0;
        size_t next_offset = 0;
        size_t count_matches = 0;
        for (; (last_match = engine.next_offset(haystack.after_n(next_offset), needle)) != not_found_k;
             count_matches++, next_offset = last_match + 1)
            callback(last_match);

        return count_matches;
    }

} // namespace av
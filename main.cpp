#include <vector>
#include <random>
#include <climits>
#include <algorithm>
#include <functional>

#include <benchmark/benchmark.h>

#include "search.hpp"

using namespace av;
namespace bm = benchmark;

constexpr float default_secs_k = 10;
constexpr size_t needle_len_k = 10;
static std::vector<uint8_t> haystack_poor;
static std::vector<uint8_t> haystack_rich;
static std::vector<span_t> needles_poor;
static std::vector<span_t> needles_rich;

span_t random_part(std::vector<uint8_t> &haystack, size_t digits) {
    span_t ret;
    ret.data = haystack.data() + rand() % (haystack.size() - digits);
    ret.len = digits;
    return ret;
}

void fill_buffer() {
    std::random_device rd;
    std::mt19937 rng(rd());
    constexpr size_t buffer_size = 1 << 29; // 512 MB of random bytes.

    haystack_rich.resize(buffer_size);
    needles_rich.resize(200);
    std::uniform_int_distribution<uint8_t> alphabet_rich('A', 'z');
    for (auto &c : haystack_rich)
        c = alphabet_rich(rng);
    for (auto &needle : needles_rich)
        needle = random_part(haystack_rich, needle_len_k);

    haystack_poor.resize(buffer_size);
    needles_poor.resize(200);
    std::uniform_int_distribution<uint8_t> alphabet_poor('a', 'z');
    for (auto &c : haystack_poor)
        c = alphabet_poor(rng);
    for (auto &needle : needles_poor)
        needle = random_part(haystack_poor, needle_len_k);
}

template <typename engine_at>
void search(bm::State &state, engine_at &&engine, bool rich) {

    if (haystack_rich.empty())
        fill_buffer();

    std::vector<uint8_t> &haystack = rich ? haystack_rich : haystack_poor;
    std::vector<span_t> &needles = rich ? needles_rich : needles_poor;
    span_t buffer_span {haystack.data(), haystack.size()};

    size_t idx_iteration = 0;
    size_t cnt_matches = 0;
    for (auto _ : state) {
        cnt_matches += enumerate_matches(buffer_span, needles[idx_iteration % needles.size()], engine, [](size_t) {});
        idx_iteration++;
    }

    state.counters["bytes/s"] = bm::Counter(idx_iteration * haystack.size(), bm::Counter::kIsRate);
    state.counters["matches/s"] = bm::Counter(cnt_matches, bm::Counter::kIsRate);
}

BENCHMARK_CAPTURE(search, stl_w_rich_alphabet, stl_t {}, true)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, naive_w_rich_alphabet, naive_t {}, true)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, prefixed_w_rich_alphabet, prefixed_t {}, true)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, prefixed_avx2_w_rich_alphabet, prefixed_avx2_t {}, true)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, hybrid_avx2_w_rich_alphabet, hybrid_avx2_t {}, true)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, speculative_avx2_w_rich_alphabet, speculative_avx2_t {}, true)->MinTime(default_secs_k);

BENCHMARK_CAPTURE(search, stl_w_poor_alphabet, stl_t {}, false)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, naive_w_poor_alphabet, naive_t {}, false)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, prefixed_w_poor_alphabet, prefixed_t {}, false)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, prefixed_avx2_w_poor_alphabet, prefixed_avx2_t {}, false)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, hybrid_avx2_w_poor_alphabet, hybrid_avx2_t {}, false)->MinTime(default_secs_k);
BENCHMARK_CAPTURE(search, speculative_avx2_w_poor_alphabet, speculative_avx2_t {}, false)->MinTime(default_secs_k);

BENCHMARK_MAIN();

#include <vector>
#include <random>
#include <climits>
#include <algorithm>
#include <functional>

#include <benchmark/benchmark.h>

#include "search.hpp"

using namespace av;
namespace bm = benchmark;

static std::vector<uint8_t> buffer_uint8s;
static std::vector<span_t> needles;

span_t random_needle() {
    constexpr size_t min_digits = 5;
    constexpr size_t max_digits = 8;
    span_t ret;
    ret.len = (rand() % (max_digits - min_digits)) + min_digits;
    ret.data = buffer_uint8s.data() + rand() % (buffer_uint8s.size() - ret.len);
    return ret;
}

void fill_buffer() {
    constexpr size_t buffer_size = 1 << 29; // 512 MB of random bytes.
    buffer_uint8s.resize(buffer_size);
    std::independent_bits_engine<std::default_random_engine, CHAR_BIT, uint8_t> generator;
    std::generate(std::begin(buffer_uint8s), std::end(buffer_uint8s), std::ref(generator));

    needles.resize(100);
    for (auto &needle : needles)
        needle = random_needle();
}

template <typename engine_at>
void run(bm::State &state, engine_at &&engine) {

    if (buffer_uint8s.empty())
        fill_buffer();

    size_t idx_iteration = 0;
    size_t cnt_matches = 0;
    span_t buffer_span {buffer_uint8s.data(), buffer_uint8s.size()};
    for (auto _ : state) {
        cnt_matches += enumerate_matches(buffer_span, needles[idx_iteration % needles.size()], engine, [](size_t) {});
        idx_iteration++;
    }

    state.SetBytesProcessed(state.iterations() * buffer_uint8s.size());
    state.counters["matches"] = bm::Counter(cnt_matches, bm::Counter::kIsRate);
}

void naive(bm::State &state) {
    run(state, naive_t {});
}
BENCHMARK(naive)->MinTime(10);

static void prefixed(bm::State &state) {
    run(state, prefixed_t {});
}
BENCHMARK(prefixed)->MinTime(10);

static void prefixed_avx2(bm::State &state) {
    run(state, prefixed_avx2_t {});
}
BENCHMARK(prefixed_avx2)->MinTime(10);

static void speculative_avx2(bm::State &state) {
    run(state, speculative_avx2_t {});
}
BENCHMARK(speculative_avx2)->MinTime(10);

static void hybrid_avx2(bm::State &state) {
    run(state, hybrid_avx2_t {});
}
BENCHMARK(hybrid_avx2)->MinTime(10);

BENCHMARK_MAIN();

#include <vector>
#include <random>
#include <climits>
#include <algorithm>
#include <functional>

#include <benchmark/benchmark.h>

#include "search.hpp"

using namespace av;
namespace bm = benchmark;

constexpr size_t count_threads_k = 22;
constexpr float default_secs_k = 1;
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

template <typename engine_at, bool rich_ak = false>
void search(bm::State &state) {

    engine_at engine;
    std::vector<uint8_t> &haystack = rich_ak ? haystack_rich : haystack_poor;
    std::vector<span_t> &needles = rich_ak ? needles_rich : needles_poor;
    span_t buffer_span {haystack.data(), haystack.size()};

    for (auto _ : state)
        enumerate_matches(buffer_span, needles[state.iterations() % needles.size()], engine, [](size_t) {});

    if (state.thread_index == 0) {
        size_t bytes_scanned = state.iterations() * haystack.size() * state.threads;
        state.counters["bytes/s/core"] = bm::Counter(bytes_scanned, bm::Counter::kAvgThreadsRate);
        state.counters["bytes/s"] = bm::Counter(bytes_scanned, bm::Counter::kIsRate);
    }
}

int main(int argc, char **argv) {

    fill_buffer();

    // Standard approaches
    bm::RegisterBenchmark("stl", search<stl_t>)->MinTime(default_secs_k);
    bm::RegisterBenchmark("naive", search<naive_t>)->MinTime(default_secs_k);
    bm::RegisterBenchmark("prefixed", search<prefixed_t>)->MinTime(default_secs_k);

    // Hardware-acceleration
#ifdef __AVX2__
    bm::RegisterBenchmark("prefixed_avx2", search<prefixed_avx2_t>)->MinTime(default_secs_k);
    bm::RegisterBenchmark("hybrid_avx2", search<hybrid_avx2_t>)->MinTime(default_secs_k);
    bm::RegisterBenchmark("speculative_avx2", search<speculative_avx2_t>)->MinTime(default_secs_k);
#endif

#ifdef __AVX512F__
    bm::RegisterBenchmark("speculative_avx512", search<speculative_avx512_t>)->MinTime(default_secs_k);
#endif

#ifdef __ARM_NEON__
    bm::RegisterBenchmark("speculative_neon", search<speculative_neon_t>)->MinTime(default_secs_k);
#endif

    // Different vocabulary size
    bm::RegisterBenchmark("naive/[a-z]", search<naive_t, false>)->MinTime(default_secs_k);
    bm::RegisterBenchmark("naive/[A-Za-z]", search<naive_t, true>)->MinTime(default_secs_k);

    // Multithreading
#ifdef __AVX2__
    bm::RegisterBenchmark("simultaneous_avx2", search<speculative_avx2_t>)
        ->MinTime(default_secs_k)
        ->MeasureProcessCPUTime()
        ->UseRealTime()
        ->Threads(1)
        ->Threads(2)
        ->Threads(count_threads_k)
        ->Threads(count_threads_k * 2);
#endif

#ifdef __AVX512F__
    bm::RegisterBenchmark("speculative_avx512", search<speculative_avx512_t>)
        ->MinTime(default_secs_k)
        ->MeasureProcessCPUTime()
        ->UseRealTime()
        ->Threads(1)
        ->Threads(2)
        ->Threads(count_threads_k)
        ->Threads(count_threads_k * 2);
#endif

    bm::Initialize(&argc, argv);
    bm::RunSpecifiedBenchmarks();
}
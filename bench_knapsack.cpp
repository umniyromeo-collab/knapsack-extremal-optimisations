// Бенчмарк 0/1-рюкзака (достижимость сумм) — 4 реализации.
#include <memory>
#include <benchmark/benchmark.h>
#include <bitset>
#include <vector>
#include <cstdint>
#include <random>

constexpr size_t NB = 1u << 17;          // capacity = 131072 бит
constexpr int    W  = (NB + 63) / 64;    // = 2048 слов

// Одинаковые предметы для всех версий (фикс. seed) — честное сравнение.
static const std::vector<int>& items() {
    static std::vector<int> v = []{
        std::mt19937 rng(42);
        std::uniform_int_distribution<int> wd(1, 3000);
        std::vector<int> t(1000);
        for (auto& x : t) x = wd(rng);
        return t;
    }();
    return v;
}

// (1) НАИВНО: vector<uint8_t>, внутренний цикл, 1 бит за итерацию
static void BM_1_naive_vector(benchmark::State& st) {
    const auto& it = items();
    for (auto _ : st) {
        std::vector<uint8_t> dp(NB, 0);
        dp[0] = 1;
        for (int x : it)
            for (int j = (int)NB - 1; j >= x; --j)
                dp[j] |= dp[j - x];
        benchmark::DoNotOptimize(dp.data());
        benchmark::ClobberMemory();
    }
}

// (2) BITSET: dp |= dp << x  — работа 64-битными словами, in-place
static void BM_2_bitset(benchmark::State& st) {
    const auto& it = items();
    for (auto _ : st) {
        auto dp = std::make_unique<std::bitset<NB>>();
        (*dp)[0] = 1;
        for (int x : it) *dp |= *dp << x;
        benchmark::DoNotOptimize(dp.get());
        benchmark::ClobberMemory();
    }
}

// сдвиг по словам (см. verify.cpp) — общий для (3) и (4)
static inline void add_item(const uint64_t* o, uint64_t* n, int x, bool par) {
    int q = x >> 6, r = x & 63;
    #pragma omp parallel for schedule(static) if(par)
    for (int w = 0; w < W; ++w) {
        uint64_t s = 0;
        if (w - q >= 0) {
            s = o[w - q] << r;
            if (r && w - q - 1 >= 0) s |= o[w - q - 1] >> (64 - r);
        }
        n[w] = o[w] | s;
    }
}

// (3) ДВУХСЛОЙНО, один поток
static void BM_3_twolayer_serial(benchmark::State& st) {
    const auto& it = items();
    for (auto _ : st) {
        std::vector<uint64_t> a(W, 0), b(W, 0);
        a[0] = 1;
        uint64_t *c = a.data(), *nx = b.data();
        for (int x : it) { add_item(c, nx, x, false); std::swap(c, nx); }
        benchmark::DoNotOptimize(c);
        benchmark::ClobberMemory();
    }
}

// (4) ДВУХСЛОЙНО, OpenMP по ядрам
static void BM_4_twolayer_omp(benchmark::State& st) {
    const auto& it = items();
    for (auto _ : st) {
        std::vector<uint64_t> a(W, 0), b(W, 0);
        a[0] = 1;
        uint64_t *c = a.data(), *nx = b.data();
        for (int x : it) { add_item(c, nx, x, true); std::swap(c, nx); }
        benchmark::DoNotOptimize(c);
        benchmark::ClobberMemory();
    }
}

BENCHMARK(BM_1_naive_vector)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_2_bitset)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_3_twolayer_serial)->Unit(benchmark::kMillisecond);
BENCHMARK(BM_4_twolayer_omp)->Unit(benchmark::kMillisecond);
BENCHMARK_MAIN();

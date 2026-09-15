# Рюкзак: наивный vector vs bitset vs двухслойный многопоточный.
# Требуется: g++ (C++20), libbenchmark-dev.
# Для профилирования (цель flamegraph): linux-perf + github.com/brendangregg/FlameGraph.

CXX      ?= g++
CXXFLAGS := -O3 -march=native -std=c++20 -fopenmp
LIBS     := -lbenchmark -lpthread

.PHONY: all bench verify why asm perfstat flamegraph clean

all: bench verify why

# --- главный бенчмарк (Google Benchmark) ---
bench: bench_knapsack.cpp
	$(CXX) $(CXXFLAGS) $< $(LIBS) -o bench_knapsack
	@echo ">>> запуск. Задать потоки: OMP_NUM_THREADS=N ./bench_knapsack"
	./bench_knapsack --benchmark_min_time=0.5s

# --- проверка корректности ручной пословной версии ---
verify: verify.cpp
	$(CXX) -O2 -std=c++20 $< -o verify && ./verify

# --- почему naive не в 64 раза медленнее: скаляр vs AVX vs bitset ---
why: why.cpp
	$(CXX) $(CXXFLAGS) $< -o why && ./why

# --- увидеть SIMD своими глазами: AVX-регистры (ymm/zmm) в дизассемблере ---
asm: why
	@echo "=== SIMD в наивном байтовом цикле (ymm = 256-бит AVX2, zmm = 512-бит AVX-512) ==="
	objdump -d -M intel why | grep -E 'vpor|vmovdqu8|zmm|ymm' | head -10

# --- диагностика memory-bound: счётчики кэш-промахов (нужен perf) ---
perfstat: bench
	perf stat -e cycles,instructions,cache-references,cache-misses,LLC-load-misses \
		./bench_knapsack --benchmark_filter=BM_4 --benchmark_min_time=1s

# --- FLAME GRAPH (нужен perf + склонированный FlameGraph рядом) ---
# git clone https://github.com/brendangregg/FlameGraph
flamegraph: bench
	perf record -F 999 -g --call-graph dwarf -- \
		./bench_knapsack --benchmark_filter=BM_1 --benchmark_min_time=2s
	perf script | ../FlameGraph/stackcollapse-perf.pl | ../FlameGraph/flamegraph.pl > flame.svg
	@echo ">>> открой flame.svg в браузере"

big: big.cpp
	$(CXX) $(CXXFLAGS) $< -o big && ./big

clean:
	rm -f bench_knapsack verify why flame.svg perf.data*

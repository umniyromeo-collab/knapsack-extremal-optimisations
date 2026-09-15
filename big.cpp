// Крупный capacity: проверяем, окупается ли многопоточность, когда
// работа на ОДИН предмет большая (барьер тонет на её фоне).
// parallel вынесен НАРУЖУ цикла = имитация пула (потоки живут всё время).
#include <vector>
#include <cstdint>
#include <chrono>
#include <cstdio>
#include <random>
#include <omp.h>
#include <bitset>
#include <memory>

constexpr size_t NB = 1u << 24;        // 16 777 216 бит (~16.7 млн) — "1e7+" capacity
constexpr int    W  = (NB + 63) / 64;  // = 262144 слова, ~2 МБ на слой
constexpr int    ITEMS = 200;

static std::vector<int> mk(){ std::mt19937 r(42); std::uniform_int_distribution<int> d(1,50000);
    std::vector<int> v(ITEMS); for(auto&x:v)x=d(r); return v; }

using clk = std::chrono::high_resolution_clock;
static double ms(clk::time_point a, clk::time_point b){ return std::chrono::duration<double,std::milli>(b-a).count(); }

// один шаг по словам
static inline void step(const uint64_t* o, uint64_t* n, int x){
    int q=x>>6, r=x&63;
    for(int w=0;w<W;++w){ uint64_t s=0; if(w-q>=0){ s=o[w-q]<<r; if(r&&w-q-1>=0) s|=o[w-q-1]>>(64-r);} n[w]=o[w]|s; }
}

double run_serial(const std::vector<int>& it){
    std::vector<uint64_t> a(W,0),b(W,0); a[0]=1; uint64_t*c=a.data(),*nx=b.data();
    auto t0=clk::now();
    for(int x:it){ step(c,nx,x); std::swap(c,nx); }
    auto t1=clk::now(); volatile uint64_t s=c[W-1];(void)s; return ms(t0,t1);
}

double run_omp(const std::vector<int>& it, int nthreads){
    std::vector<uint64_t> a(W,0),b(W,0); a[0]=1; uint64_t*c=a.data(),*nx=b.data();
    auto t0=clk::now();
    #pragma omp parallel num_threads(nthreads)     // ← бригада создаётся ОДИН раз
    {
        for(int idx=0; idx<(int)it.size(); ++idx){
            int x=it[idx]; int q=x>>6, r=x&63;
            #pragma omp for schedule(static)        // ← только раздача работы, потоки уже есть
            for(int w=0;w<W;++w){ uint64_t s=0; if(w-q>=0){ s=c[w-q]<<r; if(r&&w-q-1>=0) s|=c[w-q-1]>>(64-r);} nx[w]=c[w]|s; }
            #pragma omp single                       // барьер + swap делает один поток
            { std::swap(c,nx); }
            // неявный барьер после single — гарантирует, что все видят новый c
        }
    }
    auto t1=clk::now(); volatile uint64_t s=c[W-1];(void)s; return ms(t0,t1);
}

int main(){
    printf("capacity NB = %zu бит (%.1f млн), слов на слой W = %d (~%.1f МБ/слой), предметов = %d\n",
           NB, NB/1e6, W, W*8.0/1e6, ITEMS);
    printf("ядер доступно (omp): %d\n\n", omp_get_max_threads());
    auto it=mk();
    double sc=1e18; for(int k=0;k<3;k++) sc=std::min(sc,run_serial(it));
    printf("однопоточно (по словам): %8.2f ms\n", sc);
    for(int t : {1,2,4,8}){
        if(t>omp_get_max_threads()) break;
        double p=1e18; for(int k=0;k<3;k++) p=std::min(p,run_omp(it,t));
        printf("omp %d поток(ов)        : %8.2f ms   (ускорение над 1-поточным: x%.2f)\n", t, p, sc/p);
    }
}

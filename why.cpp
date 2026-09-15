// Почему naive не в 64 раза медленнее? Гипотеза: компилятор ВЕКТОРИЗУЕТ и байтовый цикл.
// Проверим: сравним naive СКАЛЯРНО (векторизация запрещена) vs naive с AVX vs bitset.
#include <bitset>
#include <vector>
#include <cstdint>
#include <chrono>
#include <cstdio>
#include <random>
#include <memory>

constexpr size_t NB = 1u << 17;
static std::vector<int> mk(){ std::mt19937 r(42); std::uniform_int_distribution<int> d(1,3000);
    std::vector<int> v(1000); for(auto&x:v)x=d(r); return v; }

// СКАЛЯРНО: запрещаем автовекторизацию этой функции
__attribute__((optimize("no-tree-vectorize")))
double naive_scalar(const std::vector<int>& it){
    auto dp = std::vector<uint8_t>(NB,0); dp[0]=1;
    auto t0=std::chrono::high_resolution_clock::now();
    for(int x:it) for(int j=(int)NB-1;j>=x;--j) dp[j]|=dp[j-x];
    auto t1=std::chrono::high_resolution_clock::now();
    volatile uint8_t s=dp[NB-1];(void)s;
    return std::chrono::duration<double,std::milli>(t1-t0).count();
}
// ВЕКТОРИЗОВАНО: как обычно, компилятор волен применять AVX к байтам
double naive_vec(const std::vector<int>& it){
    auto dp = std::vector<uint8_t>(NB,0); dp[0]=1;
    auto t0=std::chrono::high_resolution_clock::now();
    for(int x:it) for(int j=(int)NB-1;j>=x;--j) dp[j]|=dp[j-x];
    auto t1=std::chrono::high_resolution_clock::now();
    volatile uint8_t s=dp[NB-1];(void)s;
    return std::chrono::duration<double,std::milli>(t1-t0).count();
}
double bitset_t(const std::vector<int>& it){
    auto dp=std::make_unique<std::bitset<NB>>(); (*dp)[0]=1;
    auto t0=std::chrono::high_resolution_clock::now();
    for(int x:it) *dp|=*dp<<x;
    auto t1=std::chrono::high_resolution_clock::now();
    volatile bool s=(*dp)[NB-1];(void)s;
    return std::chrono::duration<double,std::milli>(t1-t0).count();
}
int main(){
    auto it=mk();
    // прогрев + минимум из 3
    double sc=1e9,ve=1e9,bs=1e9;
    for(int k=0;k<3;k++){ sc=std::min(sc,naive_scalar(it)); ve=std::min(ve,naive_vec(it)); bs=std::min(bs,bitset_t(it)); }
    printf("naive СКАЛЯРНО (без AVX): %7.2f ms\n", sc);
    printf("naive ВЕКТОРИЗОВАНО     : %7.2f ms   (ускорение автовекторизацией: x%.1f)\n", ve, sc/ve);
    printf("bitset                  : %7.2f ms   (над скалярным: x%.1f, над вектор.: x%.1f)\n", bs, sc/bs, ve/bs);
}

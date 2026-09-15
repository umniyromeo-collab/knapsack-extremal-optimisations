// Проверка: ручной сдвиг по 64-битным словам == std::bitset << x
#include <bitset>
#include <vector>
#include <cstdint>
#include <cstdio>
#include <random>
#include <cassert>

constexpr size_t NB = 1u << 17;          // 131072 бит (capacity)
constexpr int    W  = (NB + 63) / 64;    // число 64-битных слов

// один шаг рюкзака над массивом слов: new = old | (old << x)
// каждое выходное слово зависит ТОЛЬКО от old — значит распараллеливаемо
void add_item_words(const uint64_t* old_dp, uint64_t* new_dp, int x) {
    int q = x >> 6;      // x / 64  — сдвиг на целые слова
    int r = x & 63;      // x % 64  — сдвиг внутри слова
    for (int w = 0; w < W; ++w) {
        uint64_t shifted = 0;
        if (w - q >= 0) {
            shifted = old_dp[w - q] << r;
            if (r != 0 && w - q - 1 >= 0)
                shifted |= old_dp[w - q - 1] >> (64 - r);   // r!=0 => сдвиг в [1,63], без UB
        }
        new_dp[w] = old_dp[w] | shifted;
    }
}

int main() {
    std::mt19937 rng(123);
    std::uniform_int_distribution<int> wd(1, 3000);
    std::vector<int> items(500);
    for (auto& x : items) x = wd(rng);

    // эталон: std::bitset
    std::bitset<NB> ref;
    ref[0] = 1;
    for (int x : items) ref |= ref << x;

    // ручная версия по словам, двухслойная
    std::vector<uint64_t> a(W, 0), b(W, 0);
    a[0] = 1;
    uint64_t* cur = a.data(); uint64_t* nxt = b.data();
    for (int x : items) {
        add_item_words(cur, nxt, x);
        std::swap(cur, nxt);
    }

    // сравнить побитово
    int mism = 0;
    for (size_t i = 0; i < NB; ++i)
        if (ref[i] != ((cur[i/64] >> (i%64)) & 1)) ++mism;

    printf("несовпадений: %d\n", mism);
    printf("достижимых сумм: %zu\n", ref.count());
    return mism == 0 ? 0 : 1;
}

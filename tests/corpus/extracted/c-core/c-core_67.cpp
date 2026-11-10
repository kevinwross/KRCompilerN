#include <atomic>
int main() {
    std::atomic<int> counter;
    counter.store(5);
    if (counter.load() != 5) return 1;
    counter = 7;
    if (counter.load() != 7) return 2;
    if (counter.fetch_add(3) != 7) return 3;
    if (counter.load() != 10) return 4;
    int expected = 10;
    if (!counter.compare_exchange_weak(expected, 20)) return 5;
    if (counter.load() != 20) return 6;
    if (counter.exchange(30) != 20) return 7;
    return counter.load() == 30 ? 0 : 8;
}

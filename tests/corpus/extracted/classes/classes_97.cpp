#include <initializer_list>
struct Sum {
    long long total;
    Sum(std::initializer_list<long long> values) {
        total = 0;
        for (long long value : values) total += value;
    }
};
int main() {
    Sum sum{10000000000, 2};
    return (int)(sum.total / 1000000000);
}

#include <initializer_list>
struct Sum {
    int total;
    Sum(std::initializer_list<int> values) {
        total = 0;
        for (int value : values) total += value;
    }
};
int main() { Sum sum{1, 2, 3}; return sum.total; }

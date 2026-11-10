#include <initializer_list>
struct Count {
    int total;
    Count(std::initializer_list<int> values) {
        total = 0;
        for (int value : values) total += value;
    }
};
int main() { Count count{}; return count.total; }

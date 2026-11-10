#include <initializer_list>
int sum(std::initializer_list<int> values) {
    int total = 0;
    for (int index = 0; index < values.size_; ++index) {
        total += values.data_[index];
    }
    return total;
}
int main() { return sum({1, 2, 3, 4}); }

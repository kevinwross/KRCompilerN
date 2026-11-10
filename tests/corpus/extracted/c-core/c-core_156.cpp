#include <initializer_list>
std::initializer_list<int> make() { return {4, 5, 6}; }
int main() {
    std::initializer_list<int> values = make();
    return values.data_[0] + values.data_[2];
}

#include <utility>
template<typename T> T make_value() { return T(); }
int main() {
    if (make_value<int>() != 0) return 1;
    if (make_value<double>() != 0.0) return 2;
    std::pair<int, int> pair = make_value<std::pair<int, int> >();
    return pair.first == 0 && pair.second == 0 ? 0 : 3;
}

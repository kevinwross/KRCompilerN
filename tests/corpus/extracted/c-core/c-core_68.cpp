#include <array>
#include <utility>
int main() {
    std::array<int, 3> values;
    values[0] = 1;
    values[1] = 2;
    values[2] = 3;
    if (values.size() != 3 || values.front() != 1) return 1;
    if (values.back() != 3) return 2;
    if (values.end() - values.begin() != 3) return 3;
    std::array<double, 2> reals;
    reals[0] = 1.5;
    reals[1] = 2.5;
    if (reals[0] != 1.5 || reals.back() != 2.5) return 4;
    std::pair<int, int> pair;
    pair.first = 4;
    pair.second = 5;
    return pair.first + pair.second == 9 ? 0 : 5;
}

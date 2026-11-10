#include <vector>
#include <utility>
int main() {
    std::vector<int> values;
    values.push_back(3);
    values.push_back(5);
    values.push_back(7);
    if (values.size() != 3 || values.empty()) return 1;
    if (values[0] != 3 || values.back() != 7) return 2;
    if (values.end() - values.begin() != 3) return 3;
    values.pop_back();
    if (values.size() != 2 || values.back() != 5) return 4;
    values.clear();
    if (!values.empty()) return 5;
    values.reserve(8);
    values.push_back(11);
    if (values.capacity() < 8 || values[0] != 11) return 6;
    std::vector<double> reals;
    reals.push_back(1.5);
    reals.push_back(2.5);
    if (reals[0] != 1.5 || reals.back() != 2.5) return 7;
    std::vector<std::pair<int, int> > pairs;
    std::pair<int, int> first;
    first.first = 1;
    first.second = 2;
    pairs.push_back(first);
    return pairs[0].first == 1 && pairs[0].second == 2 ? 0 : 8;
}

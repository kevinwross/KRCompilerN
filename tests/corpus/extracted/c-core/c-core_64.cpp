#include <vector>
#include <utility>
int main() {
    std::vector<int> values;
    values.push_back(4);
    values.resize(3);
    if (values.size() != 3 || values[0] != 4) return 1;
    if (values[2] != 0) return 2;
    values.resize(1);
    if (values.size() != 1 || values[0] != 4) return 3;
    std::vector<std::pair<int, int> > pairs;
    pairs.resize(2);
    if (pairs.size() != 2) return 4;
    return pairs[1].first == 0 && pairs[1].second == 0 ? 0 : 5;
}

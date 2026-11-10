#include <vector>
#include <utility>
int main() {
    std::vector<int> first;
    first.push_back(1);
    first.push_back(2);
    std::vector<int> second = first;
    if (second.size() != 2 || second[1] != 2) return 1;
    std::vector<int> third;
    third = first;
    if (third.size() != 2 || third[0] != 1) return 2;
    third.push_back(3);
    if (third.size() != 3 || second.size() != 2) return 3;
    std::vector<std::pair<int, int> > pairs;
    std::pair<int, int> value;
    value.first = 4;
    value.second = 5;
    pairs.push_back(value);
    std::vector<std::pair<int, int> > copied = pairs;
    return copied[0].first == 4 && copied[0].second == 5 ? 0 : 4;
}

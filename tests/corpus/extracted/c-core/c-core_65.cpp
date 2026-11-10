#include <vector>
int main() {
    std::vector<int> values;
    values.push_back(1);
    values.push_back(3);
    values.insert(1, 2);
    if (values.size() != 3 || values[1] != 2) return 1;
    values.erase(1);
    return values.size() == 2 && values[0] == 1 && values[1] == 3 ? 0 : 2;
}

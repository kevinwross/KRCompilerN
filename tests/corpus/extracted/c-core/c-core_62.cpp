#include <map>
int main() {
    std::map<int, int> values;
    values[3] = 30;
    values[1] = 10;
    values[3] = 33;
    if (values.size() != 2) return 1;
    if (values[1] != 10 || values[3] != 33) return 2;
    if (values.count(3) != 1 || values.count(2) != 0) return 3;
    values[2] = 20;
    if (values.size() != 3) return 4;
    values.clear();
    return values.empty() ? 0 : 5;
}

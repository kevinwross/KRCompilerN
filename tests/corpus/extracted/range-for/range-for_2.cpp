#include <vector>
int add(int left, int right) { return left + right; }
int main() {
    int total = 0;
    for (int index = 0; index < 5; ++index) total = total + index;
    if (total != 10) return 1;
    total = add(total, 5);
    if (total != 15) return 2;
    int value = 3;
    int* pointer = &value;
    *pointer = 7;
    if (value != 7) return 3;
    std::vector<int> values;
    values.push_back(2);
    values.push_back(4);
    return values[0] + values[1] == 6 ? 0 : 4;
}

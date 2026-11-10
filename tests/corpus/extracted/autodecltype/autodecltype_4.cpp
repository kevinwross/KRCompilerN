#include <initializer_list>
int main() {
    auto values = {1, 2, 3, 4};
    int total = 0;
    for (int value : values) { total += value; }
    return total;
}

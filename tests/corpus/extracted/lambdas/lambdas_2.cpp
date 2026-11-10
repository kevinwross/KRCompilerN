#include <algorithm>
bool positive(int value) { return value > 0; }
int main() {
    int values[5] = { -1, 2, -3, 4, 5 };
    if (!std::any_of(values, values + 5, positive)) return 1;
    if (std::all_of(values, values + 5, positive)) return 2;
    if (!std::none_of(values, values + 5,
                      [](int value) { return value == 0; })) return 3;
    if (std::find_if(values, values + 5, positive) != values + 1) return 4;
    if (std::find_if_not(values, values + 5, positive) != values) return 5;
    if (std::count_if(values, values + 5, positive) != 3) return 6;
    int output[5] = { 0, 0, 0, 0, 0 };
    std::transform(values, values + 5, output,
                   [](int value) { return value * 2; });
    if (output[1] != 4 || output[4] != 10) return 7;
    int* end = std::remove_if(values, values + 5, positive);
    if (end != values + 2 || values[0] != -1 || values[1] != -3) return 8;
    std::replace_if(values, values + 5,
                    [](int value) { return value < 0; }, 0);
    if (values[0] != 0 || values[1] != 0) return 9;
    std::sort(values, values + 5,
              [](int left, int right) { return right < left; });
    return values[0] == 5 && values[4] == 0 ? 0 : 10;
}

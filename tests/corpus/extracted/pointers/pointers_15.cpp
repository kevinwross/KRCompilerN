#include <algorithm>
int main() {
    int values[5] = { 1, 2, 3, 4, 5 };
    int* middle = std::rotate(values, values + 2, values + 5);
    if (middle != values + 3) return 1;
    if (values[0] != 3 || values[2] != 5 || values[3] != 1) return 2;
    char text[5] = "abcde";
    if (std::rotate(text, text + 1, text + 5) != text + 4) return 3;
    if (text[0] != 'b' || text[4] != 'a') return 4;
    double reals[3] = { 1.5, 2.5, 3.5 };
    std::rotate(reals, reals + 2, reals + 3);
    if (reals[0] != 3.5 || reals[1] != 1.5) return 5;
    if (std::rotate(values, values, values + 3) != values + 3) return 6;
    return std::rotate(values, values + 3, values + 3) == values ? 0 : 7;
}

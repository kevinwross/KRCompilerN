#include <algorithm>
int main() {
    int values[4] = { 1, 2, 3, 4 };
    if (std::find(values, values + 4, 3) != values + 2) return 1;
    if (std::find(values, values + 4, 9) != values + 4) return 2;
    char text[4] = "abc";
    if (std::find(text, text + 3, 'b') != text + 1) return 3;
    double reals[2] = { 1.5, 2.5 };
    return std::find(reals, reals + 2, 2.5) == reals + 1 ? 0 : 4;
}

#include <algorithm>
int main() {
    int values[5] = { 1, 2, 2, 3, 2 };
    if (std::count(values, values + 5, 2) != 3) return 1;
    char text[6] = "abaca";
    if (std::count(text, text + 5, 'a') != 3) return 2;
    double reals[3] = { 1.5, 2.5, 1.5 };
    if (std::count(reals, reals + 3, 1.5) != 2) return 3;
    return std::count(reals, reals + 3, 9.0) == 0 ? 0 : 4;
}

#include <algorithm>
int main() {
    int values[4] = { 1, 2, 1, 3 };
    std::replace(values, values + 4, 1, 9);
    if (values[0] != 9 || values[1] != 2 || values[2] != 9) return 1;
    char text[5] = "aabaa";
    std::replace(text, text + 5, 'a', 'z');
    if (text[0] != 'z' || text[2] != 'b' || text[4] != 'z') return 2;
    double reals[3] = { 1.5, 2.5, 1.5 };
    std::replace(reals, reals + 3, 1.5, 9.0);
    return reals[0] == 9.0 && reals[1] == 2.5 ? 0 : 3;
}

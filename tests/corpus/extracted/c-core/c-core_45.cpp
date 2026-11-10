#include <algorithm>
int main() {
    int values[3] = { 1, 3, 2 };
    if (!std::prev_permutation(values, values + 3)) return 1;
    if (values[0] != 1 || values[1] != 2 || values[2] != 3) return 2;
    int ascending[3] = { 1, 2, 3 };
    if (std::prev_permutation(ascending, ascending + 3)) return 3;
    if (ascending[0] != 3 || ascending[2] != 1) return 4;
    char text[4] = "acb";
    if (!std::prev_permutation(text, text + 3)) return 5;
    if (text[1] != 'b' || text[2] != 'c') return 6;
    double reals[3] = { 1.5, 3.5, 2.5 };
    std::prev_permutation(reals, reals + 3);
    return reals[1] == 2.5 && reals[2] == 3.5 ? 0 : 7;
}

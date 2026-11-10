#include <algorithm>
int main() {
    int values[3] = { 1, 2, 3 };
    if (!std::next_permutation(values, values + 3)) return 1;
    if (values[0] != 1 || values[1] != 3 || values[2] != 2) return 2;
    int descending[3] = { 3, 2, 1 };
    if (std::next_permutation(descending, descending + 3)) return 3;
    if (descending[0] != 1 || descending[2] != 3) return 4;
    char text[4] = "abc";
    if (!std::next_permutation(text, text + 3)) return 5;
    if (text[1] != 'c') return 6;
    double reals[3] = { 1.5, 2.5, 3.5 };
    std::next_permutation(reals, reals + 3);
    return reals[1] == 3.5 ? 0 : 7;
}

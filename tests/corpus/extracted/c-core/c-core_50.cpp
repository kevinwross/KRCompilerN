#include <algorithm>
int main() {
    int values[6] = { 1, 3, 5, 2, 4, 6 };
    std::inplace_merge(values, values + 3, values + 6);
    if (values[0] != 1 || values[1] != 2 || values[2] != 3 || values[5] != 6) return 1;
    char text[6] = "acebdf";
    std::inplace_merge(text, text + 3, text + 6);
    if (text[0] != 'a' || text[1] != 'b' || text[5] != 'f') return 2;
    double reals[4] = { 1.5, 3.5, 2.5, 4.5 };
    std::inplace_merge(reals, reals + 2, reals + 4);
    return reals[0] == 1.5 && reals[1] == 2.5 && reals[3] == 4.5 ? 0 : 3;
}

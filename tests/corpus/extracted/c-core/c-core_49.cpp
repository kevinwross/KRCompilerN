#include <algorithm>
int main() {
    int values[4] = { 3, 1, 4, 2 };
    std::stable_sort(values, values + 4);
    if (values[0] != 1 || values[3] != 4) return 1;
    char text[5] = "dcba";
    std::stable_sort(text, text + 4);
    if (text[0] != 'a' || text[3] != 'd') return 2;
    double reals[3] = { 2.5, 0.5, 1.5 };
    std::stable_sort(reals, reals + 3);
    return reals[0] == 0.5 && reals[2] == 2.5 ? 0 : 3;
}

#include <algorithm>
int main() {
    int values[2] = { 1, 2 };
    std::iter_swap(values, values + 1);
    if (values[0] != 2 || values[1] != 1) return 1;
    char text[3] = "ab";
    std::iter_swap(text, text + 1);
    if (text[0] != 'b' || text[1] != 'a') return 2;
    double reals[2] = { 1.5, 2.5 };
    std::iter_swap(reals, reals + 1);
    return reals[0] == 2.5 && reals[1] == 1.5 ? 0 : 3;
}

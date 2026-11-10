#include <algorithm>
int main() {
    int values[6] = { 1, 2, 2, 2, 3, 4 };
    if (std::search_n(values, values + 6, 3, 2) != values + 1) return 1;
    if (std::search_n(values, values + 6, 4, 2) != values + 6) return 2;
    char text[8] = "abbcccd";
    if (std::search_n(text, text + 7, 3, 'c') != text + 3) return 3;
    double reals[4] = { 1.5, 2.5, 2.5, 3.5 };
    if (std::search_n(reals, reals + 4, 2, 2.5) != reals + 1) return 4;
    return std::search_n(text, text + 7, 0, 'z') == text ? 0 : 5;
}

#include <algorithm>
int main() {
    int values[5] = { 1, 2, 2, 3, 3 };
    if (std::adjacent_find(values, values + 5) != values + 1) return 1;
    char text[6] = "abcde";
    if (std::adjacent_find(text, text + 5) != text + 5) return 2;
    char doubled[6] = "abbcd";
    if (std::adjacent_find(doubled, doubled + 5) != doubled + 1) return 3;
    double reals[3] = { 1.5, 2.5, 2.5 };
    if (std::adjacent_find(reals, reals + 3) != reals + 1) return 4;
    return std::adjacent_find(text, text) == text ? 0 : 5;
}

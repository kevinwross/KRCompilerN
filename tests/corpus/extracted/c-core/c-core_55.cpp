#include <algorithm>
int main() {
    int sorted[4] = { 1, 2, 3, 4 };
    int unsorted[4] = { 1, 3, 2, 4 };
    if (!std::is_sorted(sorted, sorted + 4)) return 1;
    if (std::is_sorted(unsorted, unsorted + 4)) return 2;
    char text[5] = "abcd";
    if (!std::is_sorted(text, text + 4)) return 3;
    double reals[3] = { 1.5, 2.5, 3.5 };
    if (!std::is_sorted(reals, reals + 3)) return 4;
    int equal[3] = { 2, 2, 2 };
    if (!std::is_sorted(equal, equal + 3)) return 5;
    return std::is_sorted(text, text) ? 0 : 6;
}

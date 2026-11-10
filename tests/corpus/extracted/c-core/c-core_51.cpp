#include <algorithm>
int main() {
    int big[5] = { 1, 2, 3, 4, 5 };
    int small[3] = { 2, 3, 5 };
    if (!std::includes(big, big + 5, small, small + 3)) return 1;
    int missing[2] = { 2, 6 };
    if (std::includes(big, big + 5, missing, missing + 2)) return 2;
    char text[7] = "abcdef";
    char part[4] = "bde";
    if (!std::includes(text, text + 6, part, part + 3)) return 3;
    double all[4] = { 1.5, 2.5, 3.5, 4.5 };
    double some[2] = { 2.5, 4.5 };
    if (!std::includes(all, all + 4, some, some + 2)) return 4;
    return std::includes(text, text + 6, part, part) ? 0 : 5;
}

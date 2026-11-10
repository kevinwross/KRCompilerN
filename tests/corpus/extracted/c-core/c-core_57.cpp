#include <algorithm>
int main() {
    int text[7] = { 1, 2, 3, 1, 2, 3, 4 };
    int needle[2] = { 1, 2 };
    if (std::find_end(text, text + 7, needle, needle + 2) != text + 3) return 1;
    char letters[8] = "abcabcx";
    char part[3] = "abc";
    if (std::find_end(letters, letters + 7, part, part + 3) != letters + 3) return 2;
    char missing[2] = "zz";
    if (std::find_end(letters, letters + 7, missing, missing + 2) != letters + 7) return 3;
    double reals[4] = { 1.5, 2.5, 1.5, 2.5 };
    double pair[2] = { 1.5, 2.5 };
    if (std::find_end(reals, reals + 4, pair, pair + 2) != reals + 2) return 4;
    return std::find_end(letters, letters + 7, part, part) == letters + 7 ? 0 : 5;
}

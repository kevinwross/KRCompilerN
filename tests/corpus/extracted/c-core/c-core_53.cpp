#include <algorithm>
int main() {
    int text[6] = { 1, 2, 3, 4, 5, 6 };
    int set[2] = { 4, 6 };
    if (std::find_first_of(text, text + 6, set, set + 2) != text + 3) return 1;
    int missing[2] = { 8, 9 };
    if (std::find_first_of(text, text + 6, missing, missing + 2) != text + 6) return 2;
    char letters[7] = "abcdef";
    char chars[3] = "xd";
    if (std::find_first_of(letters, letters + 6, chars, chars + 2) != letters + 3) return 3;
    double reals[3] = { 1.5, 2.5, 3.5 };
    double wanted[2] = { 9.0, 2.5 };
    if (std::find_first_of(reals, reals + 3, wanted, wanted + 2) != reals + 1) return 4;
    return std::find_first_of(letters, letters + 6, chars, chars) == letters + 6 ? 0 : 5;
}

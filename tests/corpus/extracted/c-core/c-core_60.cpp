#include <algorithm>
int main() {
    int text[6] = { 1, 2, 3, 4, 5, 6 };
    int needle[2] = { 3, 4 };
    if (std::search(text, text + 6, needle, needle + 2) != text + 2) return 1;
    int missing[2] = { 8, 9 };
    if (std::search(text, text + 6, missing, missing + 2) != text + 6) return 2;
    char letters[7] = "abcdef";
    char pair[2] = "cd";
    if (std::search(letters, letters + 6, pair, pair + 2) != letters + 2) return 3;
    double reals[4] = { 1.5, 2.5, 3.5, 4.5 };
    double tail[2] = { 3.5, 4.5 };
    if (std::search(reals, reals + 4, tail, tail + 2) != reals + 2) return 4;
    return std::search(letters, letters + 6, pair, pair) == letters ? 0 : 5;
}

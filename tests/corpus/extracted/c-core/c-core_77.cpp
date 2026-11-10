#include <algorithm>
int main() {
    int values[4] = { 1, 2, 3, 4 };
    std::reverse(values, values + 4);
    if (values[0] != 4 || values[3] != 1) return 1;
    double reals[3] = { 1.5, 2.5, 3.5 };
    std::reverse(reals, reals + 3);
    if (reals[0] != 3.5 || reals[2] != 1.5) return 2;
    char text[4] = "abc";
    std::reverse(text, text + 3);
    return text[0] == 'c' && text[2] == 'a' ? 0 : 3;
}

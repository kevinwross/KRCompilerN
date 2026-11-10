#include <algorithm>
int main() {
    int values[4] = { 1, 2, 3, 4 };
    std::fill(values, values + 4, 7);
    if (values[0] != 7 || values[3] != 7) return 1;
    double reals[3] = { 0.0, 0.0, 0.0 };
    std::fill(reals, reals + 3, 1.5);
    if (reals[0] != 1.5 || reals[2] != 1.5) return 2;
    char text[4] = "abc";
    std::fill(text, text + 3, 'z');
    return text[0] == 'z' && text[2] == 'z' ? 0 : 3;
}

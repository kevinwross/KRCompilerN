#include <algorithm>
int main() {
    int values[4] = { 3, 1, 4, 2 };
    std::sort(values, values + 4);
    if (values[0] != 1 || values[3] != 4) return 1;
    double reals[3] = { 2.5, 0.5, 1.5 };
    std::sort(reals, reals + 3);
    if (reals[0] != 0.5 || reals[2] != 2.5) return 2;
    char text[4] = "dcba";
    std::sort(text, text + 4);
    return text[0] == 'a' && text[3] == 'd' ? 0 : 3;
}

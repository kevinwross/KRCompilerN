#include <algorithm>
int main() {
    int values[5] = { 1, 2, 1, 3, 1 };
    int* end = std::remove(values, values + 5, 1);
    if (end != values + 2) return 1;
    if (values[0] != 2 || values[1] != 3) return 2;
    char text[6] = "abaca";
    char* tail = std::remove(text, text + 5, 'a');
    if (tail != text + 2) return 3;
    if (text[0] != 'b' || text[1] != 'c') return 4;
    double reals[3] = { 1.5, 2.5, 1.5 };
    return std::remove(reals, reals + 3, 1.5) == reals + 1 ? 0 : 5;
}

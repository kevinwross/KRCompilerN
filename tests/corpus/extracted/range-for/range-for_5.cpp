#include <cstdlib>
#include <cstdlib>
int compare_ints(const void* left, const void* right) {
    int a = *(const int*)left;
    int b = *(const int*)right;
    return a < b ? -1 : (a > b ? 1 : 0);
}
int main() {
    if (std::atoi("42") != 42) return 1;
    if (std::atoi("-7") != -7) return 2;
    if (std::atof("2.5") != 2.5) return 3;
    char* end;
    if (std::strtol("123x", &end, 10) != 123 || end[0] != 'x') return 4;
    if (std::strtoul("ffz", &end, 16) != 255 || end[0] != 'z') return 5;
    if (std::labs(-7) != 7) return 6;
    int values[5];
    values[0] = 4; values[1] = 1; values[2] = 5;
    values[3] = 2; values[4] = 3;
    std::qsort(values, 5, sizeof(int), compare_ints);
    for (int index = 0; index < 5; ++index) {
        if (values[index] != index + 1) return 7;
    }
    int key = 3;
    int* found = (int*)std::bsearch(&key, values, 5, sizeof(int), compare_ints);
    if (found == 0 || *found != 3) return 8;
    if (std::strtod("2.5x", &end) != 2.5 || end[0] != 'x') return 9;
    if (std::strtod("1.5e2x", &end) != 150.0 || end[0] != 'x') return 10;
    if (std::atof("1e2") != 100.0) return 11;
    if (std::strtoll("-123z", &end, 10) != -123) return 12;
    if (std::strtoull("123z", &end, 10) != 123) return 13;
    if (std::llabs(-1234567890123LL) != 1234567890123LL) return 14;
    if (std::strtof("1.5x", &end) != 1.5f || end[0] != 'x') return 15;
    if (std::strtold("2.5x", &end) != 2.5L || end[0] != 'x') return 16;
    return 0;
}

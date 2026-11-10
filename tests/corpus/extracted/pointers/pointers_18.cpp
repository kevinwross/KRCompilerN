int main() {
    int values[5] = { 1, 2, 3, 4, 5 };
    int* end = values + 3;
    if (end - values != 3) return 1;
    char text[6] = "abcde";
    char* pointer = text + 4;
    if (pointer - text != 4) return 2;
    double reals[3] = { 0.0, 0.0, 0.0 };
    double* last = reals + 2;
    return last - reals == 2 ? 0 : 3;
}

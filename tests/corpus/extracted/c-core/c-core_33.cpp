int next() {
    static int value = 40;
    value = value + 1;
    return value;
}
int main() {
    if (next() != 41) return 1;
    if (next() != 42) return 2;
    return 0;
}

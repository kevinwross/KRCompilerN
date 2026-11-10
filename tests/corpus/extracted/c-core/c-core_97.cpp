long long make() { return -123; }
int main() {
    long long value = make();
    if (value != -123) return 1;
    if ((int)(value >> 32) != -1) return 2;
    return 0;
}

int main() {
    long long value = 1234567890123LL;
    if (-value != -1234567890123LL) return 1;
    if ((int)((-value) >> 32) != -288) return 2;
    return 0;
}

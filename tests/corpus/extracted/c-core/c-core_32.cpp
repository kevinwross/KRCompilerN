int main() {
    static double real = 2.5;
    static char letter = 'x';
    static long long big = 1234567890123LL;
    if (real != 2.5) return 1;
    if (letter != 'x') return 2;
    if (big != 1234567890123LL) return 3;
    return 0;
}

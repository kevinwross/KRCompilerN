int sum5(int a, int b, int c, int d, int e) {
    return a + b + c + d + e;
}
int sum6(int a, int b, int c, int d, int e, int f) {
    return a + b + c + d + e + f;
}
int main() {
    if (sum5(1, 2, 3, 4, 5) != 15) return 1;
    if (sum6(1, 2, 3, 4, 5, 6) != 21) return 2;
    int a = 1; int b = 2; int c = 3; int d = 4; int e = 5;
    return sum5(a, b, c, d, e) - 15;
}

/* kevinc declarator smoke test: pointers to functions and arrays of them. */
typedef int (*Binary)(int, int);

int plus(int a, int b) {
    return a + b;
}

int minus(int a, int b) {
    return a - b;
}

Binary choose(int which) {
    return which ? plus : minus;
}

int main(void) {
    Binary f = choose(1);
    int (*g)(int, int) = minus;
    int (*table[2])(int, int);
    table[0] = plus;
    table[1] = minus;
    return f(5, 3) + g(5, 3) + table[0](1, 2);
}

typedef int (*binary_fn)(int, int);
int add(int a, int b) { return a + b; }
int multiply(int a, int b) { return a * b; }
binary_fn choose(int which) { return which == 0 ? add : multiply; }
int apply(binary_fn operation, int a, int b) {
    return operation(a, b);
}
binary_fn global_operation = add;
int (*direct_global)(int, int) = multiply;
binary_fn operations[2] = { add, multiply };
int main(void) {
    binary_fn operation = add;
    if (operation(2, 3) != 5) return 1;
    operation = multiply;
    if (operation(2, 3) != 6) return 2;
    if (apply(add, 4, 5) != 9) return 3;
    operation = choose(1);
    if (operation(3, 4) != 12) return 4;
    if (global_operation(1, 2) != 3) return 5;
    int (*direct)(int, int) = add;
    if (direct(5, 6) != 11) return 6;
    if (direct_global(2, 4) != 8) return 7;
    if (operations[0](2, 3) != 5) return 8;
    if (operations[1](2, 4) != 8) return 9;
    return 0;
}

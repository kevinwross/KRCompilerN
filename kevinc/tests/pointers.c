/* kevinc function-pointer smoke test. */
int add(int a, int b) {
    return a + b;
}

int subtract(int a, int b) {
    return a - b;
}

int (*operation)(int, int);

int main(void) {
    int result;
    operation = add;
    result = operation(7, 3);
    operation = subtract;
    result = result + operation(7, 3);
    return result;
}

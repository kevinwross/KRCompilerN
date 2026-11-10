int add(int left, int right) { return left + right; }
int main() {
    int (*operation)(int, int) = add;
    if (operation(2, 3) != 5) return 1;
    operation = add;
    return operation(4, 5) == 9 ? 0 : 2;
}

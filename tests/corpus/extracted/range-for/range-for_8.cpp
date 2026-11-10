int sum(int count, ...) {
    va_list ap;
    __builtin_va_start(ap, count);
    int total = 0;
    for (int i = 0; i < count; ++i) {
        total += __builtin_va_arg(ap, int);
    }
    __builtin_va_end(ap);
    return total;
}
int main() { return sum(3, 10, 20, 12); }

double sum(int count, ...) {
    va_list ap;
    __builtin_va_start(ap, count);
    double total = 0.0;
    for (int i = 0; i < count; ++i) {
        total += __builtin_va_arg(ap, double);
    }
    __builtin_va_end(ap);
    return total;
}
int main() { return (int)sum(3, 10.5, 20.25f, 11.25); }

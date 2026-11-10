int value = 0;
int main() {
    int expected = 0;
    if (!__atomic_compare_exchange_n(&value, &expected, 42, 0, 0, 0)) {
        return 1;
    }
    return value;
}

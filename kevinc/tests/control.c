/* kevinc goto, do-while, and static-local smoke test. */
int counter(int limit) {
    static int calls = 0;
    int value = 0;
    calls = calls + 1;
    do {
        value++;
    } while (value < limit);
    goto finish;
    value = 99;
finish:
    return value + calls;
}

int main(void) {
    return counter(3) + counter(4);
}

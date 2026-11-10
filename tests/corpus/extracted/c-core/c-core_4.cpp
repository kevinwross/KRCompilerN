int main() {
    int line = __LINE__;
    if (line != 2) return 1;
    int first = __COUNTER__;
    int second = __COUNTER__;
    if (__DATE__[0] == 0) return 3;
    if (__TIME__[2] != ':') return 4;
    return first == 0 && second == 1 ? 0 : 2;
}

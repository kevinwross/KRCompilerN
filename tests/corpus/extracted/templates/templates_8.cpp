template <typename T> T twice(T value) { return value + value; }
template <> int twice<int>(int value) { return 42; }
int main() {
    int specialized = twice(1);
    int generic = (int)twice(1.5);
    return specialized * 10 + generic;
}

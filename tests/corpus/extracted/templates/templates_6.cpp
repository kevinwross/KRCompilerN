template <typename T> T twice(T value) { return value + value; }
template <> int twice<int>(int value) { return 42; }
int main() {
    int first = twice(1);
    int second = twice(1);
    return first + second;
}

int main() {
    int x = 1;
    auto f = [&x](int y) { x = x + y; return x; };
    int result = f(41);
    return x + result - 42;
}

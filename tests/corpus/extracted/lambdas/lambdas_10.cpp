int main() {
    int result = [](int x) { return x + 1; }(41);
    auto twice = [](int x) { return x * 2; };
    return result + twice(0);
}

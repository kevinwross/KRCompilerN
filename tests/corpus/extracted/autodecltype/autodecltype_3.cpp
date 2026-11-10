int main() {
    int a = 10;
    int b = 20;
    auto f = [=](int x) { return a + b + x; };
    a = 0;
    b = 0;
    return f(12);
}

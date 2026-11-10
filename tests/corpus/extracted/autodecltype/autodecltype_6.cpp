int* pointer() { static int value = 42; return &value; }
long long wide() { return 10000000000; }
double real() { return 2.5; }
int main() {
    auto p = pointer();
    auto w = wide();
    auto d = real();
    return *p + (int)(w / 1000000000) + (int)d;
}

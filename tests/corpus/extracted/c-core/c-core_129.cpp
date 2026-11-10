int main() {
    double positive = 10000000000.0;
    double negative = -10000000000.0;
    long long p = (long long)positive;
    long long n = (long long)negative;
    return (int)(p / 1000000000) == 10 &&
           (int)(n / 1000000000) == -10 ? 42 : 0;
}

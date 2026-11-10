int main() {
    long long positive = 10000000000;
    long long negative = -10000000000;
    double p = (double)positive;
    double n = (double)negative;
    return (int)(p / 1000000000.0) == 10 &&
           (int)(n / 1000000000.0) == -10 ? 42 : 0;
}

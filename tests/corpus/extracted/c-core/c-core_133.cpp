int main() {
    long long value = 1LL << 40;
    return (int)((value >> 40) + (1000000000LL / 1000000000));
}

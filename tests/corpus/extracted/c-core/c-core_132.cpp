int main() {
    long long value = 1;
    long long high = value << 40;
    return (high >> 40) == 1 ? 42 : 0;
}

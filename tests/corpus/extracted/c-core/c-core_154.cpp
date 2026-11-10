int main() {
    long long value = 10000000000;
    long long& ref = value;
    ref = ref + 1;
    return (int)(value / 1000000000);
}

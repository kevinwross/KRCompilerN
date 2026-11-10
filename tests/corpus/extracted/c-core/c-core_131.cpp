int main() {
    unsigned int value = 4294967295;
    long long wide = (long long)value;
    int negative = -1;
    unsigned long long sign_extended = (unsigned long long)negative;
    return (int)(wide >> 32) == 0 &&
           (int)(sign_extended >> 32) == -1 ? 42 : 0;
}

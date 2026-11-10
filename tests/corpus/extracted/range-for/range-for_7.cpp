int main() {
    double total = 0.0;
    for (int i = 0; i < 10; ++i) { total = total + 1.5; }
    if (total > 14.0) { return (int)total; }
    return 0;
}

int main() {
    try { throw 42; } catch (int value) { return value; }
    return 0;
}

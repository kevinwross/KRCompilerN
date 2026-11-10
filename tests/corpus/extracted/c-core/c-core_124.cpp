int main() {
    try {
        try { throw 42; } catch (double value) { return 1; }
    } catch (int value) { return value; }
    return 0;
}

int main() {
    try {
        try { throw 42; } catch (int value) { throw; }
    } catch (int value) { return value; }
    return 0;
}

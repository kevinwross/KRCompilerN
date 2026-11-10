int risky() { throw 42; }
int main() {
    try { return risky(); }
    catch (int value) { return value; }
    return 0;
}

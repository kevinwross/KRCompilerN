struct Counter { int value; Counter(int v) : value(v) {} int bump() { value += 2; return value; } };
int adjust(int a) { a *= 3; return a + 1; }
int main() { Counter c(1); if (c.bump() != 3) return 1; if (c.bump() != 5) return 2; return adjust(2) == 7 ? 0 : 3; }

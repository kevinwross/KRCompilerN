int leaf(int a) { int x = a + 1; return x + x + x; }
int main() { return leaf(2) == 9 ? 0 : 1; }

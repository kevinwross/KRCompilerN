int calls = 0;
int initialize() { calls = calls + 1; return 7; }
int get() { static int value = initialize(); return value; }
int main() {
    if (get() != 7) return 1;
    if (get() != 7) return 2;
    if (calls != 1) return 3;
    return 0;
}

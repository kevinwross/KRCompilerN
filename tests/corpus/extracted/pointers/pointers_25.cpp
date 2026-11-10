namespace math {
    int twice(int value) { return value * 2; }
    int twice(int value, int other) { return value * 2 + other * 2; }
}
int main() { return math::twice(5) + math::twice(1, 2); }

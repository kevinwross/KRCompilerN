class Number {
public:
    int value;
    explicit Number(int input) : value(input) {}
};
int take(Number value) { return value.value; }
int main() {
    if (take((Number)5) != 5) return 1;
    Number source(3);
    Number copy = (Number)source;
    if (copy.value != 3) return 2;
    Number direct = (Number)6;
    return direct.value - 6;
}

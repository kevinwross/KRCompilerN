class Number {
public:
    int value;
    explicit Number(int input) : value(input) {}
};
int take(Number value) { return value.value; }
int main() {
    Number direct(5);
    if (direct.value != 5) return 1;
    return take(Number(6)) - 6;
}

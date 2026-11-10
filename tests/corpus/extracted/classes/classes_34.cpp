class Number {
public:
    int value;
    explicit Number(int input) : value(input) {}
};
int main() { Number value = 5; return value.value; }

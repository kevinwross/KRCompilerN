namespace outer {
class Number {
public:
    int value;
    Number(int input) : value(input) {}
    Number& operator+=(const Number& other) {
        value = value + other.value;
        return *this;
    }
};
}
int main() {
    outer::Number left(1);
    outer::Number right(2);
    left += right;
    return left.value - 3;
}

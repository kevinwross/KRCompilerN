class Counter {
public:
    int value;
    Counter(int input) : value(input) {}
    Counter& operator+=(const Counter& other) {
        value = value + other.value;
        return *this;
    }
    Counter& add(const Counter& other) {
        (*this) += other;
        return *this;
    }
};
int main() {
    Counter left(1);
    Counter right(2);
    left.add(right);
    return left.value - 3;
}

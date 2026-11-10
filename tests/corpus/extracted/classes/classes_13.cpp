class A {
public:
    int value;
    A(int v) : value(v) {}
    int get() { return value; }
    int get() const { return value + 1; }
};
int main() {
    A a(7);
    const A b(7);
    return (a.get() - 7) + (b.get() - 8);
}

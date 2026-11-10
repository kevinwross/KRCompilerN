class Base {
public:
    int a;
    Base() { a = 40; }
};
class Derived : public Base {
public:
    int b;
    Derived() { b = 2; }
    int sum() { return a + b; }
};
int main() { Derived value; return value.sum(); }

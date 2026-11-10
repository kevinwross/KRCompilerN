class Base {
public:
    Base() { }
    virtual int value() { return 1; }
};
class Derived : public Base {
public:
    Derived() { }
    int value() override { return 42; }
};
int main() { Derived object; Base* pointer = &object; return pointer->value(); }

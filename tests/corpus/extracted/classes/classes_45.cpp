class Base {
public:
    Base() { }
    virtual int value() = 0;
};
class Derived : public Base {
public:
    int value() { return 42; }
};
int main() {
    Derived object;
    Base* pointer = &object;
    return pointer->value();
}

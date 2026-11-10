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
int main() {
    Derived object;
    Base* base = &object;
    Derived* down = dynamic_cast<Derived*>(base);
    if (down == 0) { return 1; }
    if (typeid(base) != typeid(Derived*)) { return 2; }
    return down->value();
}

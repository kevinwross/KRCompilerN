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
    Base object;
    Derived* down = dynamic_cast<Derived*>(&object);
    return down == 0 ? 42 : 0;
}

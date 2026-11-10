int base_destroyed = 0;
int derived_destroyed = 0;
class Base {
public:
    virtual ~Base() { base_destroyed = 1; }
};
class Derived : public Base {
public:
    ~Derived() { derived_destroyed = 1; }
};
int main() {
    Base* pointer = new Derived;
    delete pointer;
    return base_destroyed == 1 && derived_destroyed == 1 ? 42 : 0;
}

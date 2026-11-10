class Adder {
public:
    int base;
    Adder(int value) : base(value) {}
    int operator()(int value);
};
int Adder::operator()(int value) { return base + value; }
class Constant {
public:
    int operator()();
};
int Constant::operator()() { return 9; }
int main() {
    Adder adder(3);
    Constant constant;
    return adder(4) + constant() - 16;
}

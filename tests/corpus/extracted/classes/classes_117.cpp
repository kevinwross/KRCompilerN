class Value {
public:
    double x;
    Value(double value) : x(value) { }
    double get() { return x; }
};
int main() { Value value(21.0); return (int)(value.get() * 2.0); }

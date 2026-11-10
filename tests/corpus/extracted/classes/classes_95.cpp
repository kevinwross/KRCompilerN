struct Value {
    int number;
    Value(int value) { number = value; }
    Value(Value other) { number = other.number; }
};
int main() { Value first(42); Value second{first}; return second.number; }

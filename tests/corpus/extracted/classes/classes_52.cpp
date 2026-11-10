class Value {
public:
    int value;
    Value(int input) { value = input; }
};
int main() { Value* value = new Value(42); int result = value->value; delete value; return result; }

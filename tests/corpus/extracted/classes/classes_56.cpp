class Value { public: int value; Value(int input) { value = input; } };
int main() {
    unsigned int storage[4];
    Value* value = new (storage) Value(7);
    if (value != (Value*)storage || value->value != 7) return 1;
    int* scalar_storage = (int*)storage;
    int* scalar = new (scalar_storage) int(42);
    return *scalar == 42 ? 42 : 2;
}

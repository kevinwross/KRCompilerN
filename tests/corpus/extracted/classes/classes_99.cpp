struct Holder { int* value; int* get() { return value; } };
int main() {
    int number = 42;
    Holder holder;
    holder.value = &number;
    int* pointer = holder.get();
    return *pointer;
}

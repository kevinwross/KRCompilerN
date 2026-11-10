template <typename T> class Box {
public:
    T value;
    int kind() { return 1; }
};
template <typename T> class Box<T*> {
public:
    T* value;
    int kind() { return 2; }
};
int main() {
    Box<int*> pointer_box;
    Box<int> value_box;
    return pointer_box.kind() * 10 + value_box.kind();
}

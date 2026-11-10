template <typename T> class Box { public: int kind() { return 1; } };
template <typename T> class Box<T[3]> { public: int kind() { return 5; } };
int main() {
    Box<int[3]> array_box;
    return array_box.kind();
}

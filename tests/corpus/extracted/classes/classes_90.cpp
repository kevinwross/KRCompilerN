template <typename T> class Box { public: int kind() { return 1; } };
template <typename T> class Box<T&> { public: int kind() { return 3; } };
template <typename T> class Box<T**> { public: int kind() { return 4; } };
int main() {
    Box<int&> reference_box;
    Box<int**> pointer_box;
    return reference_box.kind() * 10 + pointer_box.kind();
}

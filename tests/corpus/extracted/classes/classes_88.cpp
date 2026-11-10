template <typename T> class Box {
public:
    T value;
    T get();
};
template <> class Box<int> {
public:
    int value;
    int get();
};
template <> int Box<int>::get() { return 42; }
int main() {
    Box<int> box;
    box.value = 1;
    return box.get();
}

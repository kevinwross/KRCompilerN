template <typename T> class Box {
public:
    T value;
    int generic() { return 1; }
};
template <> class Box<int> {
public:
    int value;
    int specialized() { return 41; }
};
int main() {
    Box<int> box;
    box.value = 1;
    return box.specialized() + box.value;
}

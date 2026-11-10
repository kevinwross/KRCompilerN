template <int N> class Box {
public:
    int data[N];
    int size() { return N; }
};
int main() {
    Box<4> box;
    box.data[3] = 42;
    return box.data[3] + box.size() - 4;
}

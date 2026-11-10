template <int N> class Box {
public:
    int size() { return N; }
};
int main() {
    Box<2 + 3> five;
    Box<1 << 3> eight;
    return five.size() * 10 + eight.size();
}

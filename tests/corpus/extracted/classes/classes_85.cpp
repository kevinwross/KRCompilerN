template <int N> class Box {
public:
    int value;
    int size() { return N; }
};
int main() {
    Box<7> seven;
    Box<9> nine;
    return seven.size() * 10 + nine.size();
}

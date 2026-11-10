namespace N {
template<typename A, typename B> class Box {
public:
    A first;
    B second;
};
}
int main() {
    N::Box<int, char> box;
    box.first = 7;
    box.second = 'x';
    return box.first == 7 && box.second == 'x' ? 0 : 1;
}

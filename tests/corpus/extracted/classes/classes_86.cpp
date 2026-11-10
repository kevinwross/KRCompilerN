template <typename T> class Box {
public:
    static T value;
};
template <typename T> T Box<T>::value = 42;
int main() {
    Box<int> box;
    return Box<int>::value == box.value ? 42 : 0;
}

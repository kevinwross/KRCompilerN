template <typename T> class Box {
public:
    T value;
    Box(T initial) { value = initial; }
    T get() { return value; }
};
int main() {
    Box<int> number(42);
    Box<char> letter('A');
    return number.get() + letter.get();
}

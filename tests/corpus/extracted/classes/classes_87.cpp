template <typename T> class Box {
public:
    T value;
    T add(T amount);
};
template <typename T> T Box<T>::add(T amount) {
    return value + amount;
}
int main() {
    Box<int> box;
    box.value = 40;
    return box.add(2);
}

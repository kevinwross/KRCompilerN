template <int N> struct Array {
    int values[N + 1];
    int size() { return N + 1; }
};
int main() {
    Array<2> array;
    array.values[2] = 42;
    return array.size() + array.values[2] - 3;
}

template <int N> struct Array {
    int values[N * 2 + 1];
    int size() { return N * 2 + 1; }
};
int main() {
    Array<2> array;
    array.values[4] = 42;
    return array.size() + array.values[4] - 6;
}

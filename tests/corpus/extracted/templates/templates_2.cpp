namespace std {
inline void arrange(int* first, int* last) { }
template<typename Compare>
inline void arrange(int* first, int* last, Compare compare) {
    int* current = first;
    if (current != last) current = current + 1;
    while (current < last) {
        int value = *current;
        int* hole = current;
        while (hole > first && compare(value, *(hole - 1))) {
            *hole = *(hole - 1);
            hole = hole - 1;
        }
        *hole = value;
        current = current + 1;
    }
}
}
int main() {
    int values[4] = { 3, 1, 4, 2 };
    std::arrange(values, values + 4,
                 [](int left, int right) { return right < left; });
    return values[0] == 4 && values[3] == 1 ? 0 : 1;
}

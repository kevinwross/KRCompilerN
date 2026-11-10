template<typename T> void sort_values(T* first, T* last) {
    T* current = first;
    if (current != last) current = current + 1;
    while (current < last) {
        T value = *current;
        T* hole = current;
        while (hole > first && value < *(hole - 1)) {
            *hole = *(hole - 1);
            hole = hole - 1;
        }
        *hole = value;
        current = current + 1;
    }
}
int main() {
    char text[4] = "dcba";
    sort_values(text, text + 4);
    if (text[0] != 'a' || text[3] != 'd') return 1;
    double reals[3] = { 2.5, 0.5, 1.5 };
    sort_values(reals, reals + 3);
    return reals[0] == 0.5 && reals[2] == 2.5 ? 0 : 2;
}

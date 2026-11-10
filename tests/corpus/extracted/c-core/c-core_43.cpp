#include <algorithm>
int main() {
    int heap[5] = { 9, 7, 8, 3, 4 };
    if (!std::is_heap(heap, heap + 5)) return 1;
    int broken[3] = { 1, 9, 8 };
    if (std::is_heap(broken, broken + 3)) return 2;
    char text[4] = "zyxw";
    if (!std::is_heap(text, text + 4)) return 3;
    double reals[3] = { 3.5, 1.5, 2.5 };
    if (!std::is_heap(reals, reals + 3)) return 4;
    int empty[1] = { 0 };
    return std::is_heap(empty, empty) ? 0 : 5;
}

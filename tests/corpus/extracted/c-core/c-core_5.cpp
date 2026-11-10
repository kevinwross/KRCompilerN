#include <utility>
#include <utility>
int main() {
    int a = 1, b = 2;
    std::swap(a, b);
    if (a != 2 || b != 1) return 1;
    double x = 1.5, y = 2.5;
    std::swap(x, y);
    if (x != 2.5 || y != 1.5) return 2;
    char c = 'a', d = 'b';
    std::swap(c, d);
    return c == 'b' && d == 'a' ? 0 : 3;
}

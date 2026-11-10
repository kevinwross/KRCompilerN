#include <algorithm>
double scale(int factor, double value) { return value * factor; }
int main() {
    double reals[3] = { 1.5, 3.5, 2.5 };
    std::sort(reals, reals + 3,
              [](double left, double right) { return right < left; });
    if (reals[0] != 3.5 || reals[2] != 1.5) return 1;
    double (*operation)(int, double) = scale;
    if (operation(3, 1.5) != 4.5) return 2;
    double (*square)(double) = [](double value) { return value * value; };
    return square(2.5) == 6.25 ? 0 : 3;
}

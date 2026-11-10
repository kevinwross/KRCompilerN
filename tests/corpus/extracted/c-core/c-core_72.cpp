#include <numeric>
int main() {
    int values[4] = { 1, 2, 3, 4 };
    if (std::accumulate(values, values + 4, 0) != 10) return 1;
    double reals[3] = { 1.5, 2.5, 3.0 };
    if (std::accumulate(reals, reals + 3, 0.0) != 7.0) return 2;
    int sequence[4] = { 0, 0, 0, 0 };
    std::iota(sequence, sequence + 4, 5);
    return sequence[0] == 5 && sequence[3] == 8 ? 0 : 3;
}

struct Accumulator {
    int add(const int& value) { return value + 1; }
    double scale(const double& value) { return value * 2.0; }
};
template<typename T> struct Holder {
    T echo(const T& value) const { return value; }
};
int main() {
    Accumulator accumulator;
    if (accumulator.add(3) != 4) return 1;
    if (accumulator.scale(1.5) != 3.0) return 2;
    Holder<int> holder;
    return holder.echo(5) == 5 ? 0 : 3;
}

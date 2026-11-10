struct Range {
    int values[3];
    int* begin() { return values; }
    int* end() { return values + 3; }
};
int main() {
    Range range;
    range.values[0] = 1;
    range.values[1] = 2;
    range.values[2] = 3;
    int sum = 0;
    for (int value : range) sum += value;
    return sum == 6 ? 0 : 1;
}

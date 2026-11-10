int sum(int* values, int count) {
    int total = 0;
    for (int i = 0; i < count; ++i) { total += values[i]; }
    return total;
}
int main() {
    int values[3];
    values[0] = 10;
    values[1] = 20;
    values[2] = 12;
    return sum(values, 3);
}

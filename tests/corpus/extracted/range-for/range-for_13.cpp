int main() {
    int values[4];
    values[0] = 1; values[1] = 2; values[2] = 3; values[3] = 4;
    int total = 0;
    for (int value : values) { total += value; }
    return total;
}

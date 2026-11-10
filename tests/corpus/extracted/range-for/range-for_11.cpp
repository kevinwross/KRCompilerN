int main() {
    int values[3] = {1, 2, 3};
    for (int& value : values) value = value + 1;
    if (values[0] != 2) return 1;
    if (values[1] != 3) return 2;
    if (values[2] != 4) return 3;
    return 0;
}

int main() {
    int values[] = {1, 2, 3, 4};
    if (sizeof(values) != 16) return 1;
    return values[3] == 4 ? 0 : 2;
}

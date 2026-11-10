int main() {
    int* values = new int[3];
    values[0] = 10; values[1] = 20; values[2] = 12;
    int result = values[0] + values[1] + values[2];
    delete[] values;
    return result;
}

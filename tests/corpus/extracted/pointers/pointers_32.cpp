int main() {
    int values[3];
    int* p = values;
    p++;
    *p = 11;
    p += 1;
    *p = 22;
    return values[1] + values[2];
}

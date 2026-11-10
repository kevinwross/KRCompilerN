int main() {
    int* value = new (std::nothrow) int[0x7FFFFFFF];
    return value == 0 ? 42 : 0;
}

int main() {
    try {
        int* value = new int[0x7FFFFFFF];
        delete[] value;
    } catch (std::bad_alloc& error) {
        return 42;
    }
    return 0;
}

#include <cstdlib>
int main() {
    int* values = (int*)std::calloc(4, sizeof(int));
    if (values == 0) return 1;
    for (int index = 0; index < 4; ++index) {
        if (values[index] != 0) return 2;
    }
    values[0] = 5;
    values = (int*)std::realloc(values, 8 * sizeof(int));
    if (values == 0 || values[0] != 5) return 3;
    std::free(values);
    void* memory = std::malloc(16);
    std::free(memory);
    return 0;
}

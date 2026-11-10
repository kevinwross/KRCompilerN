int destroyed = 0;
class Guard { public: ~Guard() { destroyed = 1; } };
int main() {
    Guard* pointer = 0;
    delete pointer;
    Guard* array = 0;
    delete[] array;
    return destroyed;
}

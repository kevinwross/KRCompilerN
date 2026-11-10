class Box {
public:
    int value;
    Box(int initial) : value(initial) {}
    Box& operator=(const char* text) { value = text[0]; return *this; }
};
int main() {
    Box box(0);
    box = "A";
    return box.value - 65;
}

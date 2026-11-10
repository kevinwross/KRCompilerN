class Box {
public:
    int value;
    Box(int initial) : value(initial) {}
    Box& operator<<(int amount);
};
Box& Box::operator<<(int amount) { value = value + amount; return *this; }
int main() { Box box(0); box << 1 << 2; return box.value; }

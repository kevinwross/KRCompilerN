class Combine {
public:
    int base;
    Combine(int value) : base(value) {}
    int operator()(int left, int right);
};
int Combine::operator()(int left, int right) {
    return base + left + right;
}
int main() {
    Combine combine(1);
    return combine(2, 3) - 6;
}

class Pair {
public:
    int a;
    int b;
};
int main() {
    Pair first; first.a = 20; first.b = 22;
    Pair second = first;
    Pair third; third = first;
    return second.a + second.b + third.a + third.b - 42;
}

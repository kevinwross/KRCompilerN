struct A { int value; A(int v) : value(v) {} };
A create() { return A(7); }
int main() {
    A a = create();
    return a.value - 7;
}

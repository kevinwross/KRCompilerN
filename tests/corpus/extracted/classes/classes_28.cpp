namespace N {
class A { public: int x; };
int read(A& value) { return value.x; }
}
int main() {
    N::A a;
    a.x = 7;
    return N::read(a) == 7 ? 0 : 1;
}

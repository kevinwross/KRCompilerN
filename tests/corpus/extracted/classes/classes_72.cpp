struct F { float value; };
struct D { double value; };
F make_f() { F object; object.value = 2.5f; return object; }
D make_d() { D object; object.value = 4.5; return object; }
float read_f(F object) { return object.value; }
double read_d(D object) { return object.value; }
int main() {
    F f = make_f();
    D d = make_d();
    return (int)(read_f(f) + read_d(d));
}

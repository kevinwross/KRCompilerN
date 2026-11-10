struct Four { float a; float b; float c; float d; };
Four make() {
    Four value;
    value.a = 1.0f; value.b = 2.0f; value.c = 3.0f; value.d = 4.0f;
    return value;
}
float sum(Four value) { return value.a + value.b + value.c + value.d; }
int main() { Four value = make(); return (int)sum(value); }

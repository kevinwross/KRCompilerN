struct Three { float a; float b; float c; };
Three make() {
    Three value; value.a = 1.0f; value.b = 2.0f; value.c = 3.0f;
    return value;
}
float sum(Three value) { return value.a + value.b + value.c; }
int main() { Three value = make(); return (int)sum(value); }

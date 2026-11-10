struct Small { int value; };
int read(Small object) { return object.value; }
int main() {
    Small object;
    object.value = 42;
    return read(object);
}

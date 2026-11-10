struct Small { int value; };
Small make() { Small object; object.value = 42; return object; }
int main() { Small object = make(); return object.value; }

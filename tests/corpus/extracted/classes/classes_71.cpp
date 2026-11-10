struct Mixed { int whole; float fraction; };
Mixed make() { Mixed value; value.whole = 40; value.fraction = 2.5f; return value; }
int read(Mixed value) { return value.whole + (int)value.fraction; }
int main() { Mixed value = make(); return read(value); }

struct Big { int whole; double fraction; };
Big make() { Big value; value.whole = 40; value.fraction = 2.5; return value; }
int read(Big value) { return value.whole + (int)value.fraction; }
int main() { Big value = make(); return read(value); }

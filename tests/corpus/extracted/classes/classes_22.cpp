int adjust(int a) { a *= 3; return a + 1; }
struct Counter { int value; int bump() { value += 2; return value; } };
int call_adjust(int x) { return adjust(x); }
int call_bump(Counter* c) { return c->bump(); }

struct S { int f() const { return 1; } };
int g(S* s) { return s->f(); }

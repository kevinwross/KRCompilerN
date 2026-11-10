struct S { int f() const noexcept { return 1; } };
int g(S* s) { return s->f(); }

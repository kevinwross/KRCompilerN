struct N { int h; N* l; N* r; };
int heightOf(N* n) { return n == 0 ? 0 : n->h; }
int maximum(int a, int b) { return a > b ? a : b; }
int f(N* n) { return maximum(heightOf(n->l), heightOf(n->r)); }
struct Box { int value; int get() { return value; } };
int g(Box* b) { return b->get(); }

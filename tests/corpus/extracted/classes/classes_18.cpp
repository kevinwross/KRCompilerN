struct Box { int value; int get() { return value; } int add(int x) { return value + x; } };
int f(Box* box) { return box->get(); }
int g(Box* box, int y) { return box->add(y); }

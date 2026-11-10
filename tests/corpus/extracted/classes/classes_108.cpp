class B { protected: int x; public: B() { x = 1; } };
class D : public B { public: int get() { return x; } };
int main() { D d; return d.get(); }

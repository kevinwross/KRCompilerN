#include <kr32/runtime.hpp>
int order = 0;
class Second {
public:
    ~Second() { order = 2; }
};
class First {
public:
    ~First() { if (order == 2) { exit(42); } }
};
First first;
Second second;
int main() { return 0; }

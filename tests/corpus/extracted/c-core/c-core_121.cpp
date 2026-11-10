#include <kr32/matrix.hpp>
int main() {
    kr32::mat4 a; a.m[0] = 5.0;
    kr32::push(a);
    kr32::mat4 b;
    kr32::pop(b);
    kr32::drop();
    return b.m[0] == 5.0 ? 42 : 0;
}

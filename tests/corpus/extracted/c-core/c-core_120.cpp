#include <kr32/matrix.hpp>
int main() {
    kr32::mat4 a; a.m[0] = 2.0; a.m[15] = 1.0;
    kr32::mat4 b; b.m[0] = 3.0; b.m[15] = 1.0;
    kr32::mat4 c; c.m[0] = 4.0;
    kr32::mat4 r = kr32::matrix_mul_add(a, b, c);
    return r.m[0] == 10.0 ? 42 : 0;
}

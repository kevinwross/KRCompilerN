#include <kr32/matrix.hpp>
int main() {
    kr32::mat4 a; a.m[0] = 2.0; a.m[5] = 2.0; a.m[10] = 2.0; a.m[15] = 2.0;
    kr32::mat4 b; b.m[0] = 3.0; b.m[5] = 3.0; b.m[10] = 3.0; b.m[15] = 3.0;
    kr32::mat4 c = kr32::matrix_mul(a, b);
    return c.m[0] == 6.0 && c.m[5] == 6.0 ? 42 : 0;
}

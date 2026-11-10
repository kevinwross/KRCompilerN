#include <kr32/matrix.hpp>
int main() {
    kr32::mat4 a; a.m[0] = 2.0; a.m[15] = 1.0;
    kr32::mat4 b = kr32::matrix_mul(a, a);
    a = kr32::matrix_mul(a, b);
    return a.m[0] == 8.0 ? 42 : 0;
}

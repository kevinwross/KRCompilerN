#include <kr32/matrix.hpp>
int main() {
    kr32::mat4* matrix = new kr32::mat4;
    if (((unsigned int)matrix & 15U) != 0) return 1;
    matrix->m[0] = 1.0;
    float value = matrix->m[0];
    delete matrix;
    return value == 1.0 ? 42 : 2;
}

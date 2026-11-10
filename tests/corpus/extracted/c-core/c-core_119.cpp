#include <kr32/matrix.hpp>
int main() {
    kr32::mat4 a; a.m[0] = 1.0; a.m[1] = 7.0; a.m[15] = 1.0;
    kr32::mat4 b = kr32::transpose(a);
    return b.m[4] == 7.0 ? 42 : 0;
}

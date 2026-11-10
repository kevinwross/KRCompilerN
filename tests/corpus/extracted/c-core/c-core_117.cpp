#include <kr32/matrix.hpp>
int main() {
    kr32::mat4 m; m.m[0] = 1.0; m.m[5] = 1.0; m.m[10] = 1.0; m.m[15] = 1.0;
    kr32::vec4 v; v.x = 2.0; v.y = 3.0; v.z = 4.0; v.w = 1.0;
    kr32::vec4 r = kr32::transform(m, v);
    return r.x == 2.0 && r.y == 3.0 && r.z == 4.0 ? 42 : 0;
}

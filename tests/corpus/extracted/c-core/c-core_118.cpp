#include <kr32/matrix.hpp>
int main() {
    kr32::vec4 a; a.x = 1.0; a.y = 2.0; a.z = 3.0; a.w = 4.0;
    kr32::vec4 b; b.x = 1.0; b.y = 1.0; b.z = 1.0; b.w = 1.0;
    return kr32::dot(a, b) == 10.0 ? 42 : 0;
}

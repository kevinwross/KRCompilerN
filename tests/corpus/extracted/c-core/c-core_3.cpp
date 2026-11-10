#include <iostream>
int main() {
    std::cout.hex();
    std::cout << 255 << std::endl;
    std::cout.dec();
    std::cout.width(4);
    std::cout << 7 << std::endl;
    std::cout.precision(2);
    std::cout << 3.14159 << std::endl;
    return 0;
}

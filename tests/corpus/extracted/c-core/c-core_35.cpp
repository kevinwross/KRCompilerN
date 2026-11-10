#include <string>
int main() {
    std::string text("hello world world");
    if (text.rfind("worldXX", 16, 5) != 12) return 1;
    if (text.rfind("worldXX", 11, 5) != 6) return 2;
    return 0;
}

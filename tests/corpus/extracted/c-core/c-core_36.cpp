#include <string>
int main() {
    std::string text("hello world");
    if (text.find("worldYY", 0, 5) != 6) return 1;
    if (text.find("worldYY", 7, 5) != -1) return 2;
    return 0;
}

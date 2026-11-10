#include <string>
int main() {
    std::string text("hello");
    std::string part(text, 1, 3);
    if (part != "ell") return 1;
    std::string tail(text, 3, 100);
    if (tail != "lo") return 2;
    std::string direct("hello", 1, 3);
    return direct == "ell" ? 0 : 3;
}

#include <string>
int main() {
    std::string text("hello");
    if (!text.contains("ell")) return 1;
    if (text.contains("xyz")) return 2;
    std::string needle("lo");
    return text.contains(needle) ? 0 : 3;
}

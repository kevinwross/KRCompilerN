#include <string>
int main() {
    std::string text("ab");
    text.reserve(100);
    if (text.capacity() < 100) return 1;
    if (text != "ab") return 2;
    return 0;
}

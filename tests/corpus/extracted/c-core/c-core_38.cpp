#include <string>
int main() {
    std::string text("abef");
    std::string part("xxcdyy");
    text.insert(2, part, 2, 2);
    if (text != "abcdef") return 1;
    text.insert(6, "QQghRR", 2, 2);
    return text == "abcdefgh" ? 0 : 2;
}

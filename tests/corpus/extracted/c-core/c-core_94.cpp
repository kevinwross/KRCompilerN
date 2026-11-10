#include <string>
int main() {
    std::string text("ab");
    std::string part("xxcdyy");
    text.append(part, 2, 2);
    if (text != "abcd") return 1;
    text.append("QQefRR", 2, 2);
    return text == "abcdef" ? 0 : 2;
}

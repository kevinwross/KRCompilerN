#include <string>
int main() {
    std::string text("abXXef");
    std::string part("yycdzz");
    text.replace(2, 2, part, 2, 2);
    if (text != "abcdef") return 1;
    text.replace(0, 2, "QQabRR", 2, 2);
    return text == "abcdef" ? 0 : 2;
}

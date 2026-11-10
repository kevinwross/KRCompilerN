#include <string>
int main() {
    char text[4] = "abc";
    text[0] = 'x';
    if (text[1] != 'b') return 1;
    char& reference = text[2];
    reference = 'z';
    if (text[2] != 'z') return 2;
    std::string value("abc");
    value.front() = 'x';
    value.back() = 'z';
    return value == "xbz" ? 0 : 3;
}

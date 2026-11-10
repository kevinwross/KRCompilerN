#include <string>
int main() {
    std::string text("hello world");
    if (text.find("world") != 6) return 1;
    std::string part = text.substr(6, 5);
    if (part != "world") return 2;
    if (text.front() != 'h') return 3;
    if (text.back() != 'd') return 4;
    if (text.rfind("l") != 9) return 5;
    if (text.compare("hello world") != 0) return 6;
    text.push_back('!');
    if (text != "hello world!") return 7;
    text.pop_back();
    if (text.find('l') != 2) return 8;
    if (text.rfind('l') != 9) return 9;
    return 0;
}

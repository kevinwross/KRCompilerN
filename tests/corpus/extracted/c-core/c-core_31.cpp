#include <string>
int check() {
    static std::string text("hello");
    text.push_back('!');
    return text.size();
}
int main() {
    if (check() != 6) return 1;
    if (check() != 7) return 2;
    return 0;
}

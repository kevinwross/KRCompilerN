#include <string>
int main() {
    std::string text("abc");
    text.replace(1, 1, 2, 'x');
    return text == "axxc" ? 0 : 1;
}

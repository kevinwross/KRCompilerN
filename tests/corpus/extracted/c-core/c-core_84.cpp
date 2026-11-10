#include <string>
int main() {
    std::string text("hello");
    text.assign(3, 'x');
    return text == "xxx" ? 0 : 1;
}

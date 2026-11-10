#include <string>
int main() {
    std::string text("hello");
    text.reserve(100);
    text.resize(2);
    text.shrink_to_fit();
    return text == "he" ? 0 : 1;
}

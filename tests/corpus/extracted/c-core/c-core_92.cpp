#include <string>
int main() {
    std::string text("a");
    text.append("b").append("c");
    return text == "abc" ? 0 : 1;
}

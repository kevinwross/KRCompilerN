#include <string>
int main() {
    std::string text("hello");
    std::string part("xxworldyy");
    text.assign(part, 2, 5);
    if (text != "world") return 1;
    text.assign("QQdoneRR", 2, 4);
    return text == "done" ? 0 : 2;
}

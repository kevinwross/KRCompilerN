#include <string>
std::string& identity(std::string& text) { return text; }
int main() {
    std::string text("a");
    identity(text) += "b";
    return text == "ab" ? 0 : 1;
}

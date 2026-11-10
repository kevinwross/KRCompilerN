#include <string>
int main() {
    std::string text("hello");
    text.erase();
    return text.empty() ? 0 : 1;
}

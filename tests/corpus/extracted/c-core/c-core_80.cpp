#include <string>
int main() {
    std::string text("hello");
    std::string prefix("he");
    std::string suffix("lo");
    if (!text.starts_with(prefix)) return 1;
    if (!text.ends_with(suffix)) return 2;
    if (text.starts_with("xy")) return 3;
    return 0;
}

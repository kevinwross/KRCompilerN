#include <string>
int main() {
    std::string text("hello world");
    if (text.find_first_of("XXoYY", 4, 5) != 4) return 1;
    if (text.find_last_of("XXoYY", 9, 5) != 7) return 2;
    if (text.find_first_not_of("XXhYY", 0, 5) != 1) return 3;
    if (text.find_last_not_of("XXdYY", 10, 5) != 9) return 4;
    return 0;
}

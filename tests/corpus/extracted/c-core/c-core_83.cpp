#include <string>
int main() {
    const std::string text("abc");
    if (text.front() != 'a') return 1;
    if (text.back() != 'c') return 2;
    return 0;
}

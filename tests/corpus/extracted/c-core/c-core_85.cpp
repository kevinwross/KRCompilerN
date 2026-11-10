#include <string>
int main() {
    std::string repeated(3, 'x');
    if (repeated != "xxx") return 1;
    std::string prefix("abcdef", 3);
    return prefix == "abc" ? 0 : 2;
}

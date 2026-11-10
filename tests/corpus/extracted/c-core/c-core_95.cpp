#include <string>
int main() {
    std::string text(3, 'x');
    return text.size() == 3 ? 0 : 1;
}

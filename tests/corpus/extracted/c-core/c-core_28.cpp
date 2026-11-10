#include <cctype>
#include <cctype>
int main() {
    if (!std::isdigit('5')) return 1;
    if (!std::isalpha('a')) return 2;
    if (!std::isalnum('7')) return 3;
    if (!std::isspace(' ')) return 4;
    if (!std::isupper('A')) return 5;
    if (!std::islower('a')) return 6;
    if (std::toupper('a') != 'A') return 7;
    if (std::tolower('A') != 'a') return 8;
    return 0;
}

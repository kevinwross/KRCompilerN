#include <cstring>
#include <cstring>
int main() {
    char buffer[16];
    std::memset(buffer, 0, 16);
    std::strcpy(buffer, "hello");
    if (std::strlen(buffer) != 5) return 1;
    if (std::strcmp(buffer, "hello") != 0) return 2;
    if (std::strncmp(buffer, "help", 3) != 0) return 3;
    if (std::strchr(buffer, 'l') == 0) return 4;
    if (std::strstr(buffer, "ell") == 0) return 5;
    std::strcat(buffer, "!");
    if (std::strcmp(buffer, "hello!") != 0) return 6;
    char copy[16];
    std::memcpy(copy, buffer, 7);
    if (std::memcmp(copy, buffer, 7) != 0) return 7;
    char partial[16];
    std::strncpy(partial, "abcdef", 3);
    partial[3] = 0;
    return std::strcmp(partial, "abc") == 0 ? 0 : 8;
}

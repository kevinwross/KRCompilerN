#include <cstring>
int main() {
    char buffer[32];
    std::memset(buffer, 0, 32);
    std::strcpy(buffer, "hello");
    std::strncat(buffer, " world", 3);
    if (std::strcmp(buffer, "hello wo") != 0) return 1;
    if (std::strrchr(buffer, 'o') == 0) return 2;
    char copy[32];
    std::memmove(copy, buffer, 9);
    if (std::memcmp(copy, buffer, 9) != 0) return 3;
    if (std::memchr(buffer, 'w', 9) == 0) return 4;
    char text[16];
    std::strcpy(text, "a,b,c");
    char* token = std::strtok(text, ",");
    if (token == 0 || token[0] != 'a') return 5;
    char* duplicate = std::strdup("copy");
    if (std::strcmp(duplicate, "copy") != 0) return 6;
    return 0;
}

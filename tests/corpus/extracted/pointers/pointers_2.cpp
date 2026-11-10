#include <cerrno>
int main() {
    std::errno = EINVAL;
    if (std::errno != EINVAL) return 1;
    const char* text = std::strerror(std::errno);
    if (text == 0 || text[0] == 0) return 2;
    if (EPERM != 1 || ENOENT != 2) return 3;
    return EACCES == 13 && ENOSPC == 28 ? 0 : 4;
}

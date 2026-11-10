#include <climits>
#include <climits>
int main() {
    if (CHAR_BIT != 8) return 1;
    if (SCHAR_MIN != -128 || SCHAR_MAX != 127) return 2;
    if (UCHAR_MAX != 255) return 3;
    if (SHRT_MIN != -32768 || SHRT_MAX != 32767) return 4;
    if (USHRT_MAX != 65535) return 5;
    if (INT_MIN != -2147483647 - 1) return 6;
    if (INT_MAX != 2147483647) return 7;
    if (UINT_MAX != 4294967295) return 8;
    return LONG_MAX == 2147483647 ? 0 : 9;
}

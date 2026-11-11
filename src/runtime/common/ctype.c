/* ctype.c — Character classification and conversion (plan.md §14.3).
 *
 * Implements isalnum, isalpha, isascii, isblank, iscntrl, isdigit, isgraph,
 * islower, isprint, ispunct, isspace, isupper, isxdigit, tolower, toupper.
 *
 * These functions are not declared as macros; they are ordinary functions
 * that can also be used as function pointers.
 */

int isalnum(int c);
int isalpha(int c);
int isascii(int c);
int isblank(int c);
int iscntrl(int c);
int isdigit(int c);
int isgraph(int c);
int islower(int c);
int isprint(int c);
int ispunct(int c);
int isspace(int c);
int isupper(int c);
int isxdigit(int c);
int tolower(int c);
int toupper(int c);

int isalnum(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') ||
           (c >= 'A' && c <= 'Z');
}

int isalpha(int c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

int isascii(int c) {
    return (unsigned char)c <= 0x7F;
}

int isblank(int c) {
    return c == ' ' || c == '\t';
}

int iscntrl(int c) {
    return (unsigned char)c < 0x20 || (unsigned char)c == 0x7F;
}

int isdigit(int c) {
    return c >= '0' && c <= '9';
}

int isgraph(int c) {
    return (unsigned char)c > 0x20 && (unsigned char)c < 0x7F;
}

int islower(int c) {
    return c >= 'a' && c <= 'z';
}

int isprint(int c) {
    return (unsigned char)c >= 0x20 && (unsigned char)c <= 0x7E;
}

int ispunct(int c) {
    return isprint(c) && !isalnum(c) && !isspace(c);
}

int isspace(int c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\v' ||
           c == '\f' || c == '\r';
}

int isupper(int c) {
    return c >= 'A' && c <= 'Z';
}

int isxdigit(int c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
           (c >= 'A' && c <= 'F');
}

int tolower(int c) {
    if (c >= 'A' && c <= 'Z') return c - 'A' + 'a';
    return c;
}

int toupper(int c) {
    if (c >= 'a' && c <= 'z') return c - 'a' + 'A';
    return c;
}

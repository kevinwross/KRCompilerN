char text[] = "abc";
int values[] = {5, 6, 7};
int main() {
    if (sizeof(text) != 4) return 1;
    if (text[2] != 'c' || text[3] != 0) return 2;
    if (sizeof(values) != 12) return 3;
    if (values[0] != 5 || values[2] != 7) return 4;
    return 0;
}

int main() {
    char buffer[8] = "abc";
    if (buffer[0] != 'a' || buffer[1] != 'b' || buffer[2] != 'c') {
        return 1;
    }
    if (buffer[3] != 0) return 2;
    char inferred[] = "abc";
    if (sizeof(inferred) != 4) return 3;
    if (inferred[2] != 'c') return 4;
    return 0;
}

int main() {
    int i = 0;
    goto check;
body:
    i += 1;
check:
    if (i < 4) goto body;
    return i;
}

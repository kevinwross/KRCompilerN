struct Pair { char tag; short value; int next; };
int sum(short* values, int count) {
    int total = 0;
    for (int index = 0; index < count; ++index) total += values[index];
    return total;
}
int main() {
    if (sizeof(short) != 2) return 1;
    if (sizeof(unsigned short) != 2) return 2;
    if (sizeof(struct Pair) != 8) return 3;
    short values[3];
    values[0] = 10; values[1] = 20; values[2] = 30;
    if (sum(values, 3) != 60) return 4;
    struct Pair pair;
    pair.tag = 'a'; pair.value = -5; pair.next = 9;
    if (pair.value != -5) return 5;
    unsigned short wrapped = 70000;
    return wrapped == 4464 ? 0 : 6;
}

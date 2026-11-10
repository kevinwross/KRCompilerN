int main() {
    int x = 1;
    int total = 0;
    switch (x) {
    case 1: total += 1;
    case 2: total += 2; break;
    case 3: total += 4; break;
    default: total += 8;
    }
    return total;
}

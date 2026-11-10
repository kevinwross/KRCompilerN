int main() {
    int x = 5;
    int i = 2;
    int result = (x > i) ? 0 : 1;
    return result + (i > x ? 10 : 20);
}

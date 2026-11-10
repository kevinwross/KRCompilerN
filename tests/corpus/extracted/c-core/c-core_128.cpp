int next() {
    static int count = 0;
    count = count + 1;
    return count;
}
int main() {
    int first = next();
    int second = next();
    return first == 1 && second == 2 ? 42 : 0;
}

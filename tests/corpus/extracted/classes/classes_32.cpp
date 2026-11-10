struct Source { int value; };
struct Target {
    int value;
    Target(Source source) : value(source.value + 1) {}
};
int take(Target target) { return target.value; }
int main() {
    Source source;
    source.value = 4;
    Target direct(source);
    if (direct.value != 5) return 1;
    Target copied = source;
    if (copied.value != 5) return 2;
    return take(source) - 5;
}

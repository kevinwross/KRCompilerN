struct Source { int value; };
struct Target {
    int value;
    Target(const Source& source) : value(source.value + 1) {}
};
int take(Target target) { return target.value; }
int main() {
    Source source;
    source.value = 4;
    Target target = (Target)source;
    Target copied = source;
    if (copied.value != 5) return 1;
    if (take(source) != 5) return 2;
    return target.value - 5;
}

struct Source { int value; };
struct Target {
    int value;
    Target() : value(0) {}
    Target(const Source& source) : value(source.value + 1) {}
};
int main() {
    Source source;
    source.value = 4;
    Target target;
    target = source;
    return target.value - 5;
}

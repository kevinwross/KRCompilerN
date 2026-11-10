struct Source { int value; };
struct Target {
    int first;
    int second;
    Target(const Source& source)
        : first(source.value + 1), second(7) {}
};
Target make(Source source) { return source; }
int main() {
    Source source;
    source.value = 4;
    Target target = make(source);
    if (target.first != 5) return 1;
    return target.second - 7;
}

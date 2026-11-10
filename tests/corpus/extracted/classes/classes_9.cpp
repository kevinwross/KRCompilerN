class Text {
public:
    const char* data;
    Text(const char* value) { data = value; }
    void append(const Text& other) { (void)other; }
};
int main() {
    Text text("hello");
    Text suffix("!");
    text.append(suffix);
    return 0;
}

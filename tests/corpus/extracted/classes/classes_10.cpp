class Text {
    const char* data_;
public:
    Text(const char* text) { data_ = text; }
    char operator[](int index) const { return data_[index]; }
};
int main() {
    Text text("hello");
    return text[0] == 'h' ? 0 : 1;
}

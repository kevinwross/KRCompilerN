#include <string>
struct Reader {
    int length(const std::string& text) const { return text.size(); }
};
int main() {
    Reader reader;
    return reader.length("hello") == 5 ? 0 : 1;
}

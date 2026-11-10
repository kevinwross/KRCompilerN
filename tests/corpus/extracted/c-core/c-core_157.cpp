#include <initializer_list>
std::initializer_list<int> empty() { return {}; }
int main() {
    std::initializer_list<int> values = empty();
    return values.size_;
}

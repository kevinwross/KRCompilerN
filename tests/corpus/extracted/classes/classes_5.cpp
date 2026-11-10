#include <iostream>
class Widget {
public:
    int value;
    Widget(int initial) : value(initial) {}
};
std::ostream& operator<<(std::ostream& out, const Widget& widget) {
    return out << widget.value;
}
int main() {
    Widget widget(42);
    std::cout << "value=" << widget << std::endl;
    return 0;
}

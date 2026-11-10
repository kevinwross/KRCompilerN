const char* name() { return __func__; }
struct Widget {
    const char* method() const { return __func__; }
};
namespace tools { const char* helper() { return __func__; } }
int main() {
    const char* function = name();
    if (function[0] != 'n' || function[3] != 'e') return 1;
    Widget widget;
    const char* method = widget.method();
    if (method[0] != 'm' || method[5] != 'd') return 2;
    const char* helper = tools::helper();
    return helper[0] == 'h' ? 0 : 3;
}

#include <initializer_list>
int main() {
    auto values = {7, 8};
    int total = 0;
    for (const int* p = values.begin(); p != values.end(); ++p) {
        total += *p;
    }
    return total;
}

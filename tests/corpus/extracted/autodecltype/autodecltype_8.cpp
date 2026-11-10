#include <initializer_list>
int main() {
    auto values = {10000000000, 2};
    return (int)(values.data_[0] / 1000000000) + values.data_[1];
}

#include <initializer_list>
int main() {
    auto values = {1, 2, 3, 4};
    return values.data_[0] + values.data_[3] + values.size_;
}

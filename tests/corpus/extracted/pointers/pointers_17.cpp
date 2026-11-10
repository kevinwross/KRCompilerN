#include <optional>
#include <string_view>
#include <utility>
int main() {
    std::optional<int> item;
    if (item.has_value() || item) return 1;
    item.set(7);
    if (!item.has_value() || item.value() != 7) return 2;
    if (*item != 7 || item.value_or(3) != 7) return 3;
    *item = 9;
    if (item.value() != 9) return 4;
    item.reset();
    if (item.value_or(3) != 3) return 5;
    item = 11;
    if (item.value() != 11) return 6;
    item.emplace();
    if (item.value() != 0) return 7;
    item.emplace(13);
    if (item.value() != 13) return 8;
    std::optional<std::pair<int, int> > pair;
    std::pair<int, int> value;
    value.first = 4;
    value.second = 5;
    pair = value;
    if (pair->first != 4 || pair->second != 5) return 9;
    pair.emplace();
    if (pair.value().second != 0) return 10;
    std::string_view view("hello");
    if (view.size() != 5 || view.empty()) return 5;
    if (view[0] != 'h' || view.back() != 'o') return 6;
    if (!view.starts_with("hell") || !view.starts_with('h')) return 7;
    if (!view.ends_with("llo") || !view.ends_with('o')) return 8;
    std::string_view tail = view.substr(1, 3);
    if (tail.size() != 3 || tail[0] != 'e') return 7;
    view.remove_prefix(1);
    view.remove_suffix(1);
    return view.size() == 3 &&
                   view.compare(std::string_view("ell")) == 0 ? 0 : 8;
}

int destroyed = 0;
class Item {
public:
    int value;
    Item() { value = 7; }
    ~Item() { destroyed = destroyed + 1; }
};
int main() {
    Item* items = new Item[3];
    int result = items[2].value;
    delete[] items;
    return destroyed == 3 ? result : 0;
}

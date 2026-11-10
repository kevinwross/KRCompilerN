int destroyed = 0;
int constructed = 0;
class Item {
public:
    Item() { if (constructed == 2) { throw 7; } constructed = constructed + 1; }
    ~Item() { destroyed = destroyed + 1; }
};
int main() {
    try { Item* items = new Item[3]; delete[] items; }
    catch (int value) { return destroyed == 2 ? 42 : 0; }
    return 0;
}

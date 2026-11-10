int destroyed = 0;
class Base {
public:
    int value;
    virtual ~Base() { destroyed = destroyed + 1; }
};
int main() {
    Base* values = new Base[2];
    values[0].value = 1;
    values[1].value = 2;
    int result = values[0].value + values[1].value;
    delete[] values;
    return result == 3 && destroyed == 2 ? 42 : 0;
}

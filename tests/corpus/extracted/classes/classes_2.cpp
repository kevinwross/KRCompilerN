class Number {
public:
    int data[2];
    Number(int value) { data[0] = value; data[1] = 0; }
    int& operator[](int index);
    Number& operator+=(int amount);
    bool operator!() const;
};
int& Number::operator[](int index) { return data[index]; }
Number& Number::operator+=(int amount) { data[0] = data[0] + amount; return *this; }
bool Number::operator!() const { return data[0] == 0; }
int main() {
    Number number(1);
    number[1] = 7;
    number += 2;
    if (!number) return 1;
    return number[0] + number[1] - 10;
}

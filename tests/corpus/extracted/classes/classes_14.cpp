class Flag {
public:
    int value;
    Flag(int input) : value(input) {}
    operator bool() const { return value != 0; }
};
class Number {
public:
    int value;
    Number(int input) : value(input) {}
    operator int() const { return value; }
};
Number global_number = 9;
int take(int input) { return input; }
bool truth(Flag flag) { return flag; }
int main() {
    Flag zero(0);
    if (zero) return 1;
    Flag one(1);
    if (!(bool)one) return 2;
    if (truth(zero)) return 3;
    if (!truth(one)) return 4;
    if (!(zero == 0)) return 5;
    if (one == 0) return 6;
    if (!(one != 0)) return 7;
    if (!(0 == zero)) return 10;
    if (!(1 == one)) return 11;
    if (!(zero < 1)) return 12;
    if (!(one > 0)) return 13;
    Number number(7);
    int converted = number;
    if (converted != 7) return 8;
    if (take(number) != 7) return 9;
    if (number + 1 != 8) return 14;
    if (1 + number != 8) return 15;
    if (number - 1 != 6) return 16;
    if (number * 2 != 14) return 17;
    if (number / 2 != 3) return 18;
    if (number % 4 != 3) return 19;
    if ((number & 1) != 1) return 20;
    if ((number * 2) != 14) return 21;
    Number implicit = 5;
    if (implicit.value != 5) return 22;
    if (take(8) != 8) return 23;
    if (global_number.value != 9) return 24;
    return (int)number - 7;
}

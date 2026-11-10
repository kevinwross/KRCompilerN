int sum(int a, int b = 2, int c = 3) { return a + b + c; }
int value(int a = 4) { return a; }
class Box {
public:
    int total;
    Box() : total(0) {}
    int add(int amount = 3) { total = total + amount; return total; }
};
class Defaulted {
public:
    int value;
    Defaulted(int input = 5) : value(input) {}
};
class Pair {
public:
    int value;
    Pair(int a, int b = 2) : value(a + b) {}
};
Defaulted defaulted_global;
int main() {
    if (sum(1) != 6) return 1;
    if (sum(1, 5) != 9) return 2;
    if (sum(1, 5, 6) != 12) return 3;
    if (value() != 4) return 4;
    Box box;
    if (box.add() != 3) return 5;
    if (box.add(4) != 7) return 6;
    Defaulted local_defaulted;
    if (local_defaulted.value != 5) return 7;
    if (defaulted_global.value != 5) return 8;
    Defaulted copy_defaulted = 4;
    if (copy_defaulted.value != 4) return 9;
    Pair pair(3);
    if (pair.value != 5) return 10;
    Pair pair_copy = 4;
    if (pair_copy.value != 6) return 11;
    return 0;
}

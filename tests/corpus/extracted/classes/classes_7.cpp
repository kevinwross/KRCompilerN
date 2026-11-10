class Counter {
public:
    int value;
    Counter(int initial) : value(initial) {}
    Counter& operator++();
    int operator++(int dummy);
};
Counter& Counter::operator++() { value = value + 1; return *this; }
int Counter::operator++(int dummy) {
    int old = value;
    value = value + 1;
    return old;
}
int main() {
    Counter counter(1);
    int old = counter++;
    if (old != 1) return 1;
    if (counter.value != 2) return 2;
    ++counter;
    return counter.value - 3;
}

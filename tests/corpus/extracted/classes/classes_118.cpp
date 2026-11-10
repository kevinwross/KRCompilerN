class Counter {
public:
    static int value;
    static int get() { return value; }
};
int Counter::value = 42;
int main() { return Counter::get(); }

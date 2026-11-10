class Counter {
public:
    int value;
    Counter() { value = 42; }
    ~Counter() { value = 0; }
};
int get() { static Counter counter; return counter.value; }
int main() { return get(); }

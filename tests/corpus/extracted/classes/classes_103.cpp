class Box {
private:
    int value;
public:
    Box() { value = 42; }
    friend int peek(Box& box);
    friend class Inspector;
};
int peek(Box& box) { return box.value; }
class Inspector { public: int get(Box& box) { return box.value; } };
int main() { Box box; Inspector inspector; return peek(box) + inspector.get(box); }

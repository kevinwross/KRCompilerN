int result = 0;
class Guard {
public:
    Guard() { }
    ~Guard() { result = 42; }
};
int main() { Guard* guard = new Guard(); delete guard; return result; }

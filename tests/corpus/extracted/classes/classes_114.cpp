int result = 0;
class Guard {
public:
    ~Guard() { result = 42; }
};
int main() { { Guard guard; } return result; }

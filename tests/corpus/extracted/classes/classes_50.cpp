int result = 0;
class Guard {
public:
    Guard() { }
    ~Guard() { result = 42; }
};
int risky() { Guard guard; throw 1; }
int main() {
    try { risky(); } catch (int value) { return result; }
    return 0;
}

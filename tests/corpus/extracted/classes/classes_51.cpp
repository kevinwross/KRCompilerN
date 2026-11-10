int result = 0;
class Guard {
public:
    Guard() { }
    ~Guard() { result = 42; }
};
int main() {
    try { Guard guard; throw 1; }
    catch (int value) { return result; }
    return 0;
}

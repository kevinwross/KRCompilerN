int result = 0;
class Guard {
public:
    int digit;
    Guard(int value) { digit = value; }
    ~Guard() { result = result * 10 + digit; }
};
void deep() { Guard guard(3); throw 1; }
void middle() { Guard guard(2); deep(); }
int main() {
    try { Guard guard(1); middle(); }
    catch (int value) { return result == 321 ? 42 : 0; }
    return 0;
}

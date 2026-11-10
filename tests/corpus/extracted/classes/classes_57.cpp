class Throwing {
public:
    Throwing() { throw 7; }
};
int main() {
    try { Throwing* value = new Throwing; }
    catch (int error) { return 42; }
    return 0;
}

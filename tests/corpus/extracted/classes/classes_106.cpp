class P {
private:
    int secret;
public:
    int x;
    P() { secret = 7; x = 1; }
    int get() { return secret; }
};
int main() { P p; p.x = 2; return p.get() + p.x; }

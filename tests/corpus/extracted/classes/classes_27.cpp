enum Color { Red, Green = 4, Blue };
enum class Mode { Off, On };
namespace N { enum Flags { None = 0, One = 1, Two = 2 }; }
class Box { public: enum Kind { Small = 1, Large = 2 }; };
int main() {
    if (Red != 0 || Green != 4 || Blue != 5) return 1;
    if (N::One != 1 || Box::Large != 2) return 5;
    int values[N::Two];
    if (sizeof(values) != 8) return 6;
    Color color = Blue;
    if (color != Blue) return 2;
    Mode mode = Mode::On;
    switch (mode) {
    case Mode::Off: return 3;
    case Mode::On: break;
    default: return 4;
    }
    return 0;
}

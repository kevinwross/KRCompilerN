class Point {
public:
    int x;
    Point(int value) : x(value) {}
    int operator+(Point other);
};
int Point::operator+(Point other) { return x + other.x; }
int main() {
    Point a(1);
    Point b(2);
    return a + b - 3;
}

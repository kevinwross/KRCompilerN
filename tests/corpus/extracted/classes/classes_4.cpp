class Point {
public:
    int x;
    Point(int value) : x(value) {}
};
int operator+(const Point& left, const Point& right) {
    return left.x + right.x;
}
int main() {
    Point a(1);
    Point b(2);
    return a + b - 3;
}

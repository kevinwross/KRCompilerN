class Point {
public:
    int x;
    int y;
    Point(int value) : x(value) { y = 1; }
    int sum() { return this->x + this->y; }
};
int main() { Point point(41); return point.sum(); }

class Point {
public:
    int x;
    int y;
    int sum() { return x + y; }
};
int main() { Point point; point.x = 20; point.y = 22; return point.sum(); }

class Point {
public:
    int x;
    int sum() { return x; }
};
int main() { Point point; point.x = 42; Point* pointer = &point; return pointer->sum(); }

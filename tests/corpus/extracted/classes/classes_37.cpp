struct Point { int x; int y; };
class Box {
public:
    Point* pointer;
    Box(Point* value) : pointer(value) {}
    Point* operator->() { return pointer; }
};
int main() {
    Point point;
    point.x = 7;
    point.y = 8;
    Box box(&point);
    if (box->x != 7) return 1;
    if (box->y != 8) return 2;
    return 0;
}

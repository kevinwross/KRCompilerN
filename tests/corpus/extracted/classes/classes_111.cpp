struct P { int x; int y; };
int main() {
    int a[2][2] = {{1, 2}, {3, 4}};
    int b[3] = {5};
    P p = {11, 22};
    return a[0][0] + a[0][1] + a[1][0] + a[1][1] + b[0] + b[2] + p.x + p.y;
}

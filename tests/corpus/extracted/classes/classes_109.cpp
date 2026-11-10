struct P { int x; int y; };
P global = {.y = 5};
int main() {
    P p = {.y = 2};
    int a[4] = {[2] = 7, 8};
    return p.x + p.y + a[0] + a[2] + a[3] + global.y;
}

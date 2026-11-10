struct P { int x; int y; };
int sum(P p) { p.x = 100; return p.x + p.y; }
P add(P a, P b) { P p; p.x = a.x + b.x; p.y = a.y + b.y; return p; }
int main() {
    P p; p.x = 1; p.y = 2;
    P q; q.x = 4; q.y = 5;
    P n = add(p, q);
    return sum(p) + p.x + n.x * 10 + n.y;
}

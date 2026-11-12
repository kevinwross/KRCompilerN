/* kevinc feature smoke test: aggregates, pointers, switch, enum, macros. */
#define N 4
#define SQUARE(x) ((x) * (x))

enum Color { RED, GREEN = 5, BLUE };

struct Point {
    int x;
    int y;
};

typedef struct Point Point;

int sum_array(int *values, int count) {
    int total = 0;
    int i;
    for (i = 0; i < count; i++) {
        total += values[i];
    }
    return total;
}

int main(void) {
    int values[N] = { 1, 2, 3, 4 };
    Point p;
    int total;
    char *text = "hello";
    p.x = 3;
    p.y = 4;
    total = sum_array(values, N);
    switch (total) {
    case 10:
        total = 1;
        break;
    case 11:
        total = 2;
        break;
    default:
        total = 3;
        break;
    }
    while (*text) {
        text++;
    }
    return SQUARE(p.x) + p.y + total + BLUE;
}

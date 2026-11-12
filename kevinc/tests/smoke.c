/* kevinc smoke test: declarations, calls, control flow, globals. */
int global_counter = 42;
static int internal_value = 7;

int add(int a, int b) {
    return a + b;
}

static int twice(int value) {
    return value * 2;
}

int main(void) {
    int x = 3;
    int y = 4;
    int z = add(x, y);
    if (z > 5) {
        z = twice(z);
    } else {
        z = 0;
    }
    while (x < 10) {
        x++;
    }
    for (y = 0; y < 3; y = y + 1) {
        z = z + y;
    }
    return z;
}

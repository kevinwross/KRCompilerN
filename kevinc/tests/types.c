/* kevinc type-system smoke test: tags, unions, enums, pointer declarators. */
struct Forward;
typedef struct Forward Forward;

struct Forward {
    int x;
};

union U {
    int i;
    char c;
};

enum E { NEG = -3, ZERO, POS = 7 };

const char *name = "kevinc";
static int *pointer_array[2];

int *return_pointer(int *p) {
    return p;
}

int apply(int (*fn)(int), int value) {
    return fn(value);
}

int double_it(int x) {
    return x * 2;
}

int main(void) {
    int a[3] = { 1, 2, 3 };
    int (*pa)[3] = &a;
    struct Forward f;
    union U u;
    short s = 6;
    unsigned char uc = 7;
    long l = 8;
    f.x = 4;
    u.i = 5;
    return (*pa)[1] + f.x + u.i + s + uc + (int)l + (int)NEG +
           return_pointer(&a[0])[0] + apply(double_it, 10);
}

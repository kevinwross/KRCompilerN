/* kevinc aggregate initializer smoke test. */
struct Pair {
    int a;
    char b;
    int c;
};

struct Pair global_pair = { 1, 'x', 3 };
int global_array[3] = { 4, 5, 6 };
char global_string[] = "abc";

int main(void) {
    struct Pair pair = { 7, 'y', 9 };
    int matrix[2][2] = { { 1, 2 }, { 3, 4 } };
    return global_pair.a + global_array[1] + global_string[0] + pair.c + matrix[1][0];
}

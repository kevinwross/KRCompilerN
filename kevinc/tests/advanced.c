/* kevinc advanced C89 smoke test: tags, typedefs, pointers, sizeof, unsigned. */
typedef struct Node {
    int value;
    struct Node *next;
} Node;

static Node nodes[2];

int compare(const void *a, const void *b) {
    return (a == b) ? 0 : 1;
}

unsigned int mix(unsigned int a, unsigned int b) {
    return (a << 2) | (b >> 1) ^ (a & b);
}

long pick(int condition, long a, long b) {
    return condition ? a : b;
}

int main(void) {
    Node *p = &nodes[0];
    int x;
    p->value = 10;
    p->next = &nodes[1];
    p->next->value = 20;
    x = (p->value > 5 && p->next != 0) ? p->value : p->next->value;
    x += (int)sizeof(Node);
    x += (int)sizeof p;
    x ^= (int)mix(3u, 4u);
    return (int)pick(x & 1, x, -x) + compare(&nodes[0], &nodes[1]);
}

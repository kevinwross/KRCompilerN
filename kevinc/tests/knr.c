/* kevinc K&R definition and loop-control smoke test. */
int sum(n)
int n;
{
    int total = 0;
    int i;
    for (i = 0; i < n; i++) {
        if (i == 2) {
            continue;
        }
        if (i == 5) {
            break;
        }
        total += i;
    }
    return total;
}

void nothing(void) {
    return;
}

int main(void) {
    nothing();
    return sum(10);
}

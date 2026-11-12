/* kevinc floating-point smoke test. */
float global_float = 1.5f;
double global_double = 2.25;

int main(void) {
    float f = 2.5f;
    double d = 3.25;
    float sum = f + (float)d;
    int i = (int)sum;
    if (d > f) {
        i += 1;
    }
    return i + (int)global_float;
}

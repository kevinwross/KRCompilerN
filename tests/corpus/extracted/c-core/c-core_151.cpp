int signed_div(int x, int y) { return x / y; }
int main() {
    unsigned int a = 1;
    unsigned int b = 2;
    unsigned int wrapped = a - b;
    int signed_result = -7 / 2;
    int shifted = (int)((unsigned)-1 >> 31);
    return (wrapped > 0) + (signed_result == -3) + shifted +
           signed_div(-7, 2) + (1u < 2u) + sizeof(unsigned int);
}

struct alignas(16) First { char value; };
alignas(32) struct Second { char value; };
int main() {
    if (sizeof(First) != 16) return 1;
    if (sizeof(Second) != 32) return 2;
    if (alignof(First) != 16) return 3;
    return alignof(Second) == 32 ? 0 : 4;
}

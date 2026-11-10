int pick(int) { return 1; }
int pick(unsigned int) { return 2; }
int main() { return pick(1u); }

long long global = 10000000000;
struct Box { long long value; };
int main() {
    Box box;
    box.value = global;
    return (int)(box.value / 1000000000);
}

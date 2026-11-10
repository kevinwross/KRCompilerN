struct Box { static long long value; };
long long Box::value = 10000000000;
int main() {
    return Box::value == 10000000000 ? 42 : 0;
}

template <typename T> struct Box { int kind() { return 1; } };
template <typename T> struct Box<Box<T>> { int kind() { return 2; } };
int main() { Box<Box<int>> value; return value.kind(); }

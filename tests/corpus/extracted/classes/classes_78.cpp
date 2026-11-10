template <typename T> struct Counter { static int value; };
template <typename T> int Counter<T>::value = 1;
template <typename T> int next() { static int local = 0; return ++local; }
int main() { return Counter<int>::value + next<int>(); }

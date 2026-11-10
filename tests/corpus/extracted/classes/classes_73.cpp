struct Pair { int first; int second; };
Pair make() { Pair pair; pair.first = 40; pair.second = 2; return pair; }
int main() { Pair pair = make(); return pair.first + pair.second; }

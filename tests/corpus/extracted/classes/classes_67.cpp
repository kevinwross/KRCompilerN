#include <stdio.h>
#include <stdlib.h>
struct Guard { ~Guard() { printf("F"); } };
Guard guard;
int main() { printf("B"); exit(7); return 0; }

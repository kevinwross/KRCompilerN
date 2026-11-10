#include <cfloat>
#include <cfloat>
int main() {
    if (FLT_DIG != 6 || DBL_DIG != 15) return 1;
    if (FLT_MANT_DIG != 24 || DBL_MANT_DIG != 53) return 2;
    float max = FLT_MAX;
    double dmax = DBL_MAX;
    if (!(max > 0.0f)) return 3;
    if (!(dmax > 1.0)) return 4;
    if (!(FLT_MIN > 0.0f)) return 5;
    if (!(DBL_MIN > 0.0)) return 6;
    return 0;
}

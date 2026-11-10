int main() {
    int* values = new int[4]{1, 2, 3};
    if (values[0] != 1 || values[2] != 3 || values[3] != 0) return 1;
    delete[] values;
    int* zeroed = new int[2]();
    if (zeroed[0] != 0 || zeroed[1] != 0) return 2;
    delete[] zeroed;
    float* floats = new float[2]{1.5f, 2.5f};
    if (floats[0] != 1.5f || floats[1] != 2.5f) return 3;
    delete[] floats;
    double* aligned = new double(2.5);
    if (((unsigned int)aligned & 7U) != 0 || *aligned != 2.5) return 4;
    delete aligned;
    return 42;
}

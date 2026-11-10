int main() {
    double value = 3.75;
    int whole = (int)value;
    float small = (float)whole;
    double widened = (double)small;
    return (int)widened + 39;
}

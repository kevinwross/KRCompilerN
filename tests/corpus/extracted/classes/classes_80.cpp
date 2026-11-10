template <int N, int M> struct Grid {
    int values[N + M];
    int size() { return N + M; }
};
template <int N, int M> struct Product {
    int values[(N + M) * 2];
    int size() { return N * 2 + M * 2; }
};
int main() {
    Grid<2, 3> grid;
    grid.values[4] = 40;
    Product<1, 2> product;
    product.values[5] = 2;
    return grid.size() + product.size() +
           grid.values[4] + product.values[5] - 11;
}

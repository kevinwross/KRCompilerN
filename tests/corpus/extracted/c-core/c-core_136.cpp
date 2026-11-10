int counter = 0;
int main() {
    __atomic_store_n(&counter, 1, 0);
    __atomic_fetch_add(&counter, 41, 0);
    return __atomic_load_n(&counter, 0);
}

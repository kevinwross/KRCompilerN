#include <thread>
int main() {
    int value = 42;
    std::thread worker([value]() { return value; }, 0);
    worker.join();
    return 0;
}

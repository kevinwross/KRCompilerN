#include <thread>
int worker(int* value) { return *value; }
int main() {
    std::thread a(worker, 1);
    std::thread b([](int value) { return value; }, 2);
    std::thread c([](int value) { return value; }, 3);
    c.detach();
    return a.joinable() + b.joinable();
}

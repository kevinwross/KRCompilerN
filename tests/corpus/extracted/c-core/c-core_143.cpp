#include <thread>
int main() {
    std::this_thread::sleep_until(0);
    return 42;
}

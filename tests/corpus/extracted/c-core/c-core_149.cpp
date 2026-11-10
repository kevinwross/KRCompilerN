#include <mutex>
#include <thread>
int main() {
    std::mutex guard;
    if (!guard.try_lock()) return 1;
    guard.unlock();
    std::recursive_mutex recursive;
    recursive.lock();
    recursive.lock();
    recursive.unlock();
    recursive.unlock();
    return std::thread::hardware_concurrency() > 0 ? 0 : 2;
}

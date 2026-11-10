#include <thread>
#include <mutex>
int main() {
    std::mutex guard;
    guard.lock();
    bool acquired = guard.try_lock_for(0);
    guard.unlock();
    std::this_thread::sleep_for(0);
    return acquired ? 0 : 42;
}

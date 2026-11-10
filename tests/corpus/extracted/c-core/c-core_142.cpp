#include <thread>
#include <mutex>
#include <condition_variable>
int main() {
    std::mutex guard;
    std::condition_variable condition;
    guard.lock();
    condition.wait_for(guard, 0);
    condition.wait_until(guard, 0);
    guard.unlock();
    return 42;
}

#include <thread>
#include <mutex>
#include <condition_variable>
int main() {
    std::thread thread;
    std::mutex guard;
    std::condition_variable condition;
    std::this_thread::yield();
    if (thread.joinable()) return 1;
    return 0;
}

#include <kr32/thread.hpp>
int main() {
    kr32::atomic_int value;
    value.store(1);
    value.fetch_add(41);
    kr32::mutex guard;
    guard.lock();
    guard.unlock();
    return value.load();
}

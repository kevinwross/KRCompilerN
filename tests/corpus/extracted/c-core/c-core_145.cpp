#include <thread>
int main() {
    std::thread thread;
    std::thread::id id = thread.get_id();
    return id == 0 ? 42 : 0;
}

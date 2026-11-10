#include <thread>
int main() {
    std::thread::id id = std::this_thread::get_id();
    std::thread::id copy = id;
    return copy == id ? 42 : 0;
}

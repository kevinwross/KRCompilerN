#include <kr32/thread.hpp>
int main() {
    kr32::thread thread;
    kr32::thread::id id = thread.get_id();
    return id == 0 ? 42 : 0;
}

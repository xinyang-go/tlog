#include "tlog/tlog.hpp"
#include <thread>

using namespace std::chrono_literals;

// only print to remote 
[[noreturn]] void remote_print(std::string info, int id) {
    tlog::tbuf buf(std::to_string(id));
    std::ostream tout(&buf);

    int i = 0;
    while (true) {
        tout << "[" << std::this_thread::get_id() << "](" << i << "): " << info << std::endl;
        i++;
        std::this_thread::sleep_for(1s);
    }
}

// echo string received to remote
void remote_echo(int id) {
    tlog::tbuf buf(std::to_string(id));
    std::ostream tout(&buf);
    std::istream tin(&buf);

    int i = 0;
    std::string str;
    while (true) {
        tin >> str;
        tout << "[" << std::this_thread::get_id() << "](" << i << "): " << str << std::endl;
        i++;
        if (str == "exit") break;
    }
}

int main() {
    std::thread tid[4];
    for (int i = 0; i < 4; i++) {
        if (i % 2) tid[i] = std::thread(remote_print, "hello world!", i);
        else tid[i] = std::thread(remote_echo, i);
    }
    for (int i = 0; i < 4; i++) {
        tid[i].join();
    }

    return 0;
}

#include "tlog/tlog.hpp"
#include <unistd.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>

extern "C" void relay(int fd, const char* name);

tlog::tbuf::tbuf(std::string name) : name(std::move(name)) {
    if (socketpair(AF_UNIX, SOCK_STREAM, 0, _fd) == -1) [[unlikely]] {
        throw std::runtime_error("[tbuf]: socketpair failed!");
    }
    pid_t pid = fork();
    if (pid < 0) [[unlikely]] {
        throw std::runtime_error("[tbuf]: fork failed!");
    } else if (pid == 0) {
        close(_fd[0]);
        write(_fd[1], &pid, sizeof(pid));
        relay(_fd[1], this->name.c_str());
    } else {
        read(_fd[0], &child_pid, sizeof(child_pid));
        close(_fd[1]);
    }
}

tlog::tbuf::~tbuf() {
    close(_fd[0]);
    int stat;
    waitpid(child_pid, &stat, 0);
}

std::streamsize tlog::tbuf::xsputn(const char *s, std::streamsize size) {
    auto len = write(_fd[0], s, size);
    if (len <= 0) [[unlikely]] throw std::runtime_error("[tlog]: error read subprocess!");
    return len;
}

std::streamsize tlog::tbuf::xsgetn(char *s, std::streamsize size) {
    auto len = read(_fd[0], s, size);
    if (len <= 0) [[unlikely]] throw std::runtime_error("[tlog]: error read subprocess!");
    return len;
}

int tlog::tbuf::underflow() {
    auto len = read(_fd[0], buf, sizeof(buf));
    if (len <= 0) [[unlikely]] throw std::runtime_error("[tlog]: error read subprocess!");
    setg(buf, buf, buf + len);
    return buf[0];
}

int tlog::tbuf::overflow(int c) {
    char ch = c;
    auto len = write(_fd[0], &ch, sizeof(ch));
    if (len <= 0) [[unlikely]] throw std::runtime_error("[tlog]: error read subprocess!");
    return c;
}

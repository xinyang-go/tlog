#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>

#ifndef NDEBUG
#define LOGD(fmt, ...) printf("[DEBUG]: "fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) printf("[INFO]: "fmt, ##__VA_ARGS__)
#else // NDEBUG
#define LOGD(fmt, ...) ((void)0)
#define LOGI(fmt, ...) printf("[INFO]: "fmt, ##__VA_ARGS__)
#endif


#define PORT_BEGIN  (10550)

int try_bind(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("socket() error!\n");
        exit(-1);
    }
    struct sockaddr_in addr_in = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(fd, (struct sockaddr *) &addr_in, sizeof(struct sockaddr)) < 0) {
        close(fd);
        return -1;
    }
    return fd;
}

void notify(pid_t pid, int port) {
    union sigval value = {
            .sival_int = port,
    };
    LOGD("notify port=%d\n", port);
    if (sigqueue(pid, SIGUSR1, value) < 0) {
        printf("sigqueue() error!\n");
        exit(-1);
    }
}

void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

void relay(int fd) {
    set_nonblock(fd);
    set_nonblock(0);

    uint8_t buffer[256];
    int size;

    fd_set ifds;
    while (1) {
        FD_ZERO(&ifds);
        FD_SET(fd, &ifds);
        FD_SET(0, &ifds);

        switch (select(fd + 1, &ifds, 0, 0, 0)) {
            case -1:
                printf("select() error!\n");
                exit(-1);
            case 0:
                break;
            default:
                if (FD_ISSET(fd, &ifds)) {
                    size = read(fd, buffer, sizeof(buffer) - 1);
                    if (size <= 0) {
                        LOGI("connection close!\n");
                        exit(0);
                    }
                    buffer[size] = 0;
                    size = write(1, buffer, size);
                }
                if (FD_ISSET(0, &ifds)) {
                    size = read(0, buffer, sizeof(buffer) - 1);
                    buffer[size] = 0;
                    size = write(fd, buffer, size);
                }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("usage: %s <pid>\n", argv[0]);
        return -1;
    }
    pid_t pid = atoi(argv[1]);

    int socket_fd = -1;
    int port = PORT_BEGIN;
    do {
        socket_fd = try_bind(++port);
    } while (socket_fd == -1);

    if (listen(socket_fd, 1) < 0) {
        printf("listen() error!\n");
        exit(-1);
    }

    notify(pid, port);

    struct sockaddr_in addr_in;
    socklen_t sin_size;
    int connect_fd = accept(socket_fd, (struct sockaddr *) &addr_in, &sin_size);
    if (connect_fd < 0) {
        printf("accept() error!\n");
        exit(-1);
    }

    LOGI("accept connection!\n");

    relay(connect_fd);

    return 0;
}

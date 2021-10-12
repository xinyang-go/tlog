#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h> 

#ifndef NDEBUG
#define LOGD(fmt, ...) printf("[DEBUG]: "fmt, ##__VA_ARGS__)
#define LOGI(fmt, ...) printf("[INFO]: "fmt, ##__VA_ARGS__)
#else // NDEBUG
#define LOGD(fmt, ...) ((void)0)
#define LOGI(fmt, ...) printf("[INFO]: "fmt, ##__VA_ARGS__)
#endif

#define BOARDCAST_PORT  (28888)

int try_bind() {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("socket() error!\n");
        exit(-1);
    }
    struct sockaddr_in addr_in = {
            .sin_family = AF_INET,
            .sin_port = htons(0),
            .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if (bind(fd, (struct sockaddr *) &addr_in, sizeof(struct sockaddr)) < 0) {
        printf("bind() error!\n");
        exit(-1);
    }
    return fd;
}

void notify(const char* name, int socket_fd) {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0) {
        printf("socket() error!\n");
    }
    int opt = -1;
    if(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt,sizeof(opt)) < 0) {
        printf("setsockopt() error!\n");
    }

    struct sockaddr_in addr_this;
    socklen_t size = sizeof(addr_this);
    getsockname(socket_fd, (struct sockaddr *)&addr_this, &size);
    char msg[128];
    int len = snprintf(msg, sizeof(msg), "%s %d %s", name, 
                       ntohs(addr_this.sin_port), inet_ntoa(addr_this.sin_addr));

    struct sockaddr_in addr_to = {
        .sin_family = AF_INET,
        .sin_port = htons(BOARDCAST_PORT),
        .sin_addr.s_addr = htonl(INADDR_BROADCAST),
    };    
    sendto(fd, msg, len, 0, (struct sockaddr*)&addr_to, sizeof(addr_to));
    close(fd);
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
        printf("usage: %s <name>\n", argv[0]);
        return -1;
    }
    const char* name = argv[1];

    int socket_fd = try_bind();    

    if (listen(socket_fd, 1) < 0) {
        printf("listen() error!\n");
        exit(-1);
    }

    notify(name, socket_fd);

    struct sockaddr_in addr_in;
    socklen_t sin_size = sizeof(addr_in);
    int connect_fd = accept(socket_fd, (struct sockaddr *) &addr_in, &sin_size);
    if (connect_fd < 0) {
        exit(-1);
    }

    LOGI("accept connection!\n");

    relay(connect_fd);

    return 0;
}

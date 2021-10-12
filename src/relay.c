#include <fcntl.h>
#include <dirent.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

static void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

static int remote_port = -1;

static int try_connect(int port) {
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        printf("socket() error!\n");
        exit(-1);
    }

    struct sockaddr_in addr_in = {
            .sin_family = AF_INET,
            .sin_port = htons(port),
            .sin_addr.s_addr = inet_addr("127.0.0.1"),
    };

    if (connect(socket_fd, (struct sockaddr *) &addr_in, sizeof(struct sockaddr)) < 0) {
        printf("connect() error!\n");
        exit(-1);
    }

    return socket_fd;
}

static void handler(int sig, siginfo_t *info, void *ctx) {
    if (sig == SIGUSR1) {
        remote_port = info->si_value.sival_int;
    }
}

static void wait_signal() {
    struct sigaction act = {
            .sa_sigaction = handler,
            .sa_flags = SA_SIGINFO,
    };
    sigemptyset(&act.sa_mask);

    if (sigaction(SIGUSR1, &act, NULL) < 0) {
        printf("sigaction() error!\n");
        exit(-1);
    }
}

static void close_unused_fd(int fd) {
    int other_fd;
    DIR *d;
    struct dirent *dir;
    d = opendir("/dev/fd/");
    if (d) {
        while ((dir = readdir(d)) != NULL) {
            if (strcmp(dir->d_name, ".") == 0) continue;
            if (strcmp(dir->d_name, "..") == 0) continue;
            other_fd = atoi(dir->d_name);
            if (other_fd <= 2) continue;
            if (other_fd == fd) continue;
            close(other_fd);
        }
        closedir(d);
    }
}

void relay(int fd) {
    printf("[tlog]: relay process pid=%d\n", getpid());
    close_unused_fd(fd);
    set_nonblock(fd);
    wait_signal();

    char buffer[256];
    int size;

    int remote_fd = -1;
    fd_set ifds;
    int maxfd;
    while (1) {
        if (remote_fd < 0 && remote_port > 0) {
            remote_fd = try_connect(remote_port);
        }

        FD_ZERO(&ifds);
        FD_SET(fd, &ifds);
        if (remote_fd > 0) {
            FD_SET(remote_fd, &ifds);
            maxfd = remote_fd > fd ? remote_fd : fd;
        } else {
            maxfd = fd;
        }

        switch (select(maxfd + 1, &ifds, 0, 0, 0)) {
            case -1:
                break;
            case 0:
                break;
            default:
                if (FD_ISSET(fd, &ifds)) {
                    size = read(fd, buffer, sizeof(buffer) - 1);
                    buffer[size] = 0;
                    buffer[size] = 0;
                    if (size <= 0) {
                        exit(-1);
                    }
                    if (remote_fd > 0) {
                        size = write(remote_fd, buffer, size);
                    }
                }
                if (remote_fd > 0 && FD_ISSET(remote_fd, &ifds)) {
                    size = read(remote_fd, buffer, sizeof(buffer) - 1);
                    buffer[size] = 0;
                    if (size <= 0) {
                        close(remote_fd);
                        remote_port = remote_fd = -1;
                        break;
                    }
                    size = write(fd, buffer, size);
                }
        }
    }
}

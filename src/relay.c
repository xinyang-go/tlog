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

#define BOARDCAST_PORT  (28888)

struct remote_info {
    const char* name;
    const char* ip;
    int port;
};

static void set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    flags |= O_NONBLOCK;
    fcntl(fd, F_SETFL, flags);
}

static int try_connect(const struct remote_info* info) {
    // printf("[DEBUG]: try_connect(ip=%s, port=%d)\n", info->ip, info->port);
    int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        printf("socket() error!\n");
        exit(-1);
    }

    struct sockaddr_in addr_in = {
            .sin_family = AF_INET,
            .sin_port = htons(info->port),
            .sin_addr.s_addr = inet_addr(info->ip),
    };
    if (connect(socket_fd, (struct sockaddr *) &addr_in, sizeof(struct sockaddr)) < 0) {
        close(socket_fd);
        return -1;
    }
    return socket_fd;
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

static int wait_boardcast() {
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if(fd < 0) {
        printf("socket() error!\n");
        exit(-1);
    }
    int opt=-1;
    if(setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
        printf("setsockopt() error!\n");
        exit(-1);
    }
    if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        printf("setsockopt() error!\n");
        exit(-1);
    }

    struct sockaddr_in addr_bc = {
        .sin_family = AF_INET,
        .sin_port = htons(BOARDCAST_PORT),
        .sin_addr.s_addr = htonl(INADDR_ANY),
    };
    if(bind(fd, (struct sockaddr*)&addr_bc, sizeof(addr_bc)) < 0) {
        printf("bind() error!\n");
        exit(-1);
    }
    set_nonblock(fd);
    return fd;
}

int check_name(const char* buffer, const char* name) {
    while(*buffer != 0 && *name != 0) {
        if(*buffer++ != *name++) return 0;
    }
    if(*name == 0 && *buffer == ' ') return 1;
    return 0;
}

int parse_boardcast(char* buffer, struct remote_info* info) {
    // printf("[DEBUG]: parse_boardcast(buffer='%s')\n", buffer);
    char* begin = buffer;
    for(;*buffer != 0; buffer++) {
        if(*buffer == ' ') {
            info->name = begin;
            *buffer = 0;
            info->port = atoi(buffer+1);
            return 1;
        }
    }
    return 0;
}

void relay(int fd, const char* name) {
    // printf("[tlog]: relay process pid=%d\n", getpid());
    close_unused_fd(fd);
    set_nonblock(fd);
    int bc_fd = wait_boardcast();

    int name_size = strlen(name);

    char buffer[256];
    int size;

    struct remote_info bc_info = {
        .ip = "127.0.0.1"
    };

    int remote_fd = -1;
    fd_set ifds;
    int maxfd;
    while (1) {
        FD_ZERO(&ifds);
        FD_SET(fd, &ifds);
        FD_SET(bc_fd, &ifds);
        maxfd = bc_fd > fd ? bc_fd : fd;
        if (remote_fd > 0) {
            FD_SET(remote_fd, &ifds);
            maxfd = remote_fd > maxfd ? remote_fd : maxfd;
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
                        remote_fd = -1;
                        break;
                    }
                    size = write(fd, buffer, size);
                }
                if(FD_ISSET(bc_fd, &ifds)) {
                    size = read(bc_fd, buffer, sizeof(buffer) - 1);
                    buffer[size] = 0;
                    if(parse_boardcast(buffer, &bc_info)) {
                        if(strcmp(bc_info.name, name) == 0) {
                            if(remote_fd < 0) {
                                remote_fd = try_connect(&bc_info);
                            }
                        }
                    }                    
                }
        }
    }
}

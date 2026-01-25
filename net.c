// net.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include "common.h"
#include "net.h"

void init_net_module(void) {
    printf("%s[NET MODULE]%s Initializing BSD Sockets...\n", COLOR_CYAN, COLOR_RESET);
}

int net_socket_create(void) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        printf("%s[NET ERROR]%s Failed to create socket: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
        return -1;
    }
    int opt = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    printf("%s[NET]%s Socket created (fd=%d)\n", COLOR_GREEN, COLOR_RESET, fd);
    return fd;
}

void net_connect_to(int fd, const char* ip, int port) {
    if (fd < 0 || !ip) return;

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(port);

    if (inet_pton(AF_INET, ip, &serv_addr.sin_addr) <= 0) {
        printf("%s[NET ERROR]%s Invalid address: %s\n", COLOR_RED, COLOR_RESET, ip);
        return;
    }

    if (connect(fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("%s[NET ERROR]%s Connection failed to %s:%d (%s)\n", 
               COLOR_RED, COLOR_RESET, ip, port, strerror(errno));
    } else {
        printf("%s[NET]%s Connected to %s:%d\n", COLOR_GREEN, COLOR_RESET, ip, port);
    }
}

int net_start_listen(int port) {
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        printf("%s[NET ERROR]%s Socket creation failed\n", COLOR_RED, COLOR_RESET);
        return -1;
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("%s[NET ERROR]%s Bind failed on port %d: %s\n", 
               COLOR_RED, COLOR_RESET, port, strerror(errno));
        close(server_fd);
        return -1;
    }

    if (listen(server_fd, 3) < 0) {
        printf("%s[NET ERROR]%s Listen failed\n", COLOR_RED, COLOR_RESET);
        close(server_fd);
        return -1;
    }

    printf("%s[NET]%s Server listening on port %d (fd=%d)\n", 
           COLOR_GREEN, COLOR_RESET, port, server_fd);
    return server_fd;
}

int net_accept_client(int server_fd) {
    if (server_fd < 0) return -1;

    struct sockaddr_in address;
    int addrlen = sizeof(address);
    
    printf("%s[NET]%s Waiting for connection on fd=%d...\n", COLOR_CYAN, COLOR_RESET, server_fd);
    
    int new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
    if (new_socket < 0) {
        printf("%s[NET ERROR]%s Accept failed: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
        return -1;
    }

    printf("%s[NET]%s Accepted connection from %s:%d (fd=%d)\n", 
           COLOR_GREEN, COLOR_RESET, 
           inet_ntoa(address.sin_addr), ntohs(address.sin_port), new_socket);

    return new_socket;
}

void net_send_data(int fd, const char* data) {
    if (fd < 0 || !data) return;

    ssize_t sent = send(fd, data, strlen(data), 0);
    if (sent < 0) {
        printf("%s[NET ERROR]%s Send failed: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
    } else {
        printf("%s[NET]%s Sent %zd bytes\n", COLOR_GREEN, COLOR_RESET, sent);
    }
}

char* net_recv_data(int fd, int size) {
    if (fd < 0) return NULL;
    if (size > 65535) size = 65535;
    
    char* buffer = malloc(size + 1);
    ssize_t valread = recv(fd, buffer, size, 0);
    
    if (valread > 0) {
        buffer[valread] = '\0';
        return buffer;
    } else {
        free(buffer);
        return NULL;
    }
}

void net_close_socket(int fd) {
    if (fd >= 0) {
        close(fd);
        printf("%s[NET]%s Closed socket fd=%d\n", COLOR_GREEN, COLOR_RESET, fd);
    }
}

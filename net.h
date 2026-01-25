// net.h
#ifndef NET_H
#define NET_H

#include "common.h"

void init_net_module(void);

// Nouvelles signatures avec types primitifs
int net_socket_create(void);
void net_connect_to(int fd, const char* ip, int port);
int net_start_listen(int port);
int net_accept_client(int server_fd);
void net_send_data(int fd, const char* data);
char* net_recv_data(int fd, int size);
void net_close_socket(int fd);

#endif

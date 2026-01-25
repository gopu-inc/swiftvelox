// net.h
#ifndef NET_H
#define NET_H

#include "common.h"

void init_net_module(void);
void net_socket(ASTNode* node);
void net_connect(ASTNode* node);
void net_listen(ASTNode* node);
void net_accept(ASTNode* node);
void net_send(ASTNode* node);
void net_recv(ASTNode* node);
void net_close(ASTNode* node);

#endif

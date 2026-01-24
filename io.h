// io.h - Header pour le module IO
#ifndef IO_H
#define IO_H

#include "common.h"

// Prototypes des fonctions IO
void init_io_module(void);
void io_open(ASTNode* node);
void io_close(ASTNode* node);
void io_read(ASTNode* node);
void io_write(ASTNode* node);
void io_seek(ASTNode* node);
void io_tell(ASTNode* node);
void io_exists(ASTNode* node);
void io_isfile(ASTNode* node);
void io_isdir(ASTNode* node);
void io_mkdir(ASTNode* node);
void io_listdir(ASTNode* node);

#endif // IO_H

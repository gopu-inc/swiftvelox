// io.h - Header pour le module IO
#ifndef IO_H
#define IO_H

#include "common.h"
#include <stdbool.h>

// Initialisation du module
void init_io_module(void);

// ============================================================
// FONCTIONS INSTRUCTIONS (Statements)
// Gérées par execute() dans swf.c (void)
// ============================================================
void io_open(ASTNode* node);
void io_close(ASTNode* node);
void io_write(ASTNode* node);
void io_seek(ASTNode* node);
void io_tell(ASTNode* node);
void io_flush(ASTNode* node);

// Gestion Fichiers/Dossiers (Mode instruction)
void io_mkdir(ASTNode* node);
void io_rmdir(ASTNode* node);
void io_listdir(ASTNode* node);
void io_remove(ASTNode* node);
void io_rename(ASTNode* node);
void io_copy(ASTNode* node);

// Anciennes versions (gardées pour éviter les erreurs si io.c les contient encore)
void io_read(ASTNode* node);
void io_exists(ASTNode* node);
void io_isfile(ASTNode* node);
void io_isdir(ASTNode* node);

// ============================================================
// NOUVELLES FONCTIONS EXPRESSIONS (Return Values)
// Gérées par evalString/evalFloat dans swf.c
// C'est celles-ci qui manquaient au linker !
// ============================================================

// Retourne true (1) si le fichier existe, false (0) sinon
bool io_exists_bool(const char* path);

// Lit un fichier et retourne son contenu (ou NULL)
char* io_read_string(const char* path);

#endif // IO_H


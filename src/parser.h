#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "value.h"

typedef struct ASTNode {
    int type;  // Pour l'instant, simple placeholder
    void* data;
} ASTNode;

// Fonctions de parsing (stubs pour l'instant)
ASTNode* parse(const char* source);
Value eval(ASTNode* node, Environment* env);

#endif

#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "value.h"

typedef struct ASTNode {
    NodeType type;
    Token token;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children;
    int child_count;
    ValueType inferred_type;
} ASTNode;

// Fonctions du parser
ASTNode* parse(const char* source);
ASTNode* new_node(NodeType type);
ASTNode* expression();
ASTNode* statement();
ASTNode* declaration();

#endif

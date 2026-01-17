#include "parser.h"
#include <stdio.h>
#include <stdlib.h>

ASTNode* parse(const char* source) {
    printf("📝 Parsing en développement...\n");
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = 0;
    node->data = NULL;
    return node;
}

Value eval(ASTNode* node, Environment* env) {
    printf("⚡ Évaluation en développement...\n");
    return make_nil();
}

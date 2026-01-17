#ifndef PARSER_H
#define PARSER_H

#include "lexer.h"
#include "value.h"

// ===== PARSER =====
typedef enum {
    NODE_PROGRAM, NODE_BLOCK, NODE_VAR_DECL, NODE_CONST_DECL,
    NODE_FUNCTION, NODE_CLASS, NODE_METHOD, NODE_CONSTRUCTOR,
    NODE_IF, NODE_ELIF, NODE_ELSE, NODE_UNLESS,
    NODE_FOR, NODE_FOR_IN, NODE_WHILE, NODE_DO_WHILE,
    NODE_MATCH, NODE_CASE, NODE_DEFAULT,
    NODE_TRY, NODE_CATCH, NODE_FINALLY, NODE_THROW,
    NODE_RETURN, NODE_YIELD, NODE_BREAK, NODE_CONTINUE,
    NODE_CALL, NODE_NEW, NODE_SUPER, NODE_THIS,
    NODE_ACCESS, NODE_INDEX, NODE_SLICE,
    NODE_BINARY, NODE_UNARY, NODE_TERNARY,
    NODE_ASSIGN, NODE_COMPOUND_ASSIGN,
    NODE_LITERAL, NODE_IDENTIFIER, NODE_TEMPLATE,
    NODE_ARRAY, NODE_OBJECT, NODE_PROPERTY,
    NODE_SPREAD, NODE_REST, NODE_DESTRUCTURING,
    NODE_LAMBDA, NODE_ARROW_FUNC,
    NODE_ASYNC, NODE_AWAIT,
    NODE_IMPORT, NODE_EXPORT, NODE_MODULE,
    NODE_TYPE_DECL, NODE_GENERIC, NODE_UNION_TYPE,
    NODE_DECORATOR, NODE_ANNOTATION,
    NODE_MACRO_DEF, NODE_MACRO_CALL,
    NODE_PATTERN, NODE_WILDCARD, NODE_BIND_PATTERN,
    NODE_ARRAY_PATTERN, NODE_OBJECT_PATTERN,
    NODE_GUARD, NODE_WHERE,
    NODE_PIPELINE, NODE_COMPOSITION,
    NODE_COMPREHENSION, NODE_GENERATOR,
    NODE_TEST, NODE_ASSERT, NODE_BENCH,
    NODE_DEBUG, NODE_TRACE,
    NODE_EXPR_STMT, NODE_LOOP_BODY, NODE_NULLABLE_TYPE,
    NODE_IMPLEMENTS, NODE_EXPR
} NodeType;

struct ASTNode {
    NodeType type;
    Token token;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children;
    int child_count;
    ValueType inferred_type;
};

struct Macro {
    char* name;
    char** params;
    int param_count;
    ASTNode* body;
    Environment* env;
};

// Fonctions du parser
ASTNode* parse(const char* source);
ASTNode* new_node(NodeType type);
ASTNode* expression();
ASTNode* statement();
ASTNode* declaration();
ASTNode* function_expression();
ASTNode* type_annotation();
ASTNode* pattern();
ASTNode* expression_statement();

// Variables globales du parser
extern Macro macros[MAX_MACROS];
extern int macro_count;
extern Package* packages[MAX_PACKAGES];
extern int package_count;

#endif

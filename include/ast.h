#ifndef AST_H
#define AST_H

#include "common.h"

// AST Node creation functions
ASTNode* ast_new_node(NodeType type, int line, int column);
ASTNode* ast_new_int(int64_t value, int line, int column);
ASTNode* ast_new_float(double value, int line, int column);
ASTNode* ast_new_string(const char* value, int line, int column);
ASTNode* ast_new_bool(bool value, int line, int column);
ASTNode* ast_new_identifier(const char* name, int line, int column);
ASTNode* ast_new_binary(NodeType type, ASTNode* left, ASTNode* right, int line, int column);
ASTNode* ast_new_unary(NodeType type, ASTNode* operand, int line, int column);
ASTNode* ast_new_assignment(ASTNode* left, ASTNode* right, int line, int column);
ASTNode* ast_new_var_decl(const char* name, ASTNode* value, TokenKind var_type, int line, int column);
ASTNode* ast_new_if(ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch, int line, int column);
ASTNode* ast_new_while(ASTNode* condition, ASTNode* body, int line, int column);
ASTNode* ast_new_for(ASTNode* init, ASTNode* condition, ASTNode* update, ASTNode* body, int line, int column);
ASTNode* ast_new_function(const char* name, ASTNode* params, ASTNode* body, int line, int column);
ASTNode* ast_new_function_call(ASTNode* function, ASTNode* args, int line, int column);
ASTNode* ast_new_return(ASTNode* value, int line, int column);
ASTNode* ast_new_import(char** modules, int count, const char* from_module, int line, int column);
ASTNode* ast_new_print(ASTNode* value, int line, int column);
ASTNode* ast_new_input(const char* prompt, int line, int column);

// AST traversal and utility
void ast_free(ASTNode* node);
void ast_print(ASTNode* node, int indent);
const char* node_type_to_string(NodeType type);
const char* token_kind_to_string(TokenKind kind);

// Optimizer
ASTNode* ast_optimize(ASTNode* node);

#endif // AST_H

#ifndef PARSER_H
#define PARSER_H

#include "common.h"
#include "ast.h"

// Parser functions (only public API)
void parser_init(Parser* parser, Lexer* lexer);
ASTNode* parse_program(Parser* parser);
ASTNode* parse_statement(Parser* parser);
ASTNode* parse_block(Parser* parser);
void parser_synchronize(Parser* parser);
bool parser_match(Parser* parser, TokenKind kind);
bool parser_check(Parser* parser, TokenKind kind);
Token parser_consume(Parser* parser, TokenKind kind, const char* error_message);
void parser_error(Parser* parser, Token token, const char* message);
void parser_error_at_current(Parser* parser, const char* message);
Token parser_advance(Parser* parser);

// Grammar-specific parsing (public)
ASTNode* parse_import_statement(Parser* parser);
ASTNode* parse_if_statement(Parser* parser);
ASTNode* parse_while_statement(Parser* parser);
ASTNode* parse_for_statement(Parser* parser);
ASTNode* parse_var_declaration(Parser* parser);
ASTNode* parse_function_declaration(Parser* parser);
ASTNode* parse_print_statement(Parser* parser);
ASTNode* parse_input_statement(Parser* parser);

#endif // PARSER_H

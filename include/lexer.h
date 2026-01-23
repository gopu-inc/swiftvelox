#ifndef LEXER_H
#define LEXER_H

#include "common.h"

typedef struct {
    const char* source;
    const char* start;
    const char* current;
    int line;
    int column;
    Token current_token;
    char* filename;
} Lexer;

// Lexer functions
void lexer_init(Lexer* lexer, const char* source, const char* filename);
Token lexer_next_token(Lexer* lexer);
Token lexer_peek_token(Lexer* lexer);
void lexer_skip_whitespace(Lexer* lexer);
bool lexer_is_at_end(Lexer* lexer);
char lexer_advance(Lexer* lexer);
char lexer_peek(Lexer* lexer);
char lexer_peek_next(Lexer* lexer);
bool lexer_match(Lexer* lexer, char expected);
Token lexer_make_token(Lexer* lexer, TokenKind kind);
Token lexer_error_token(Lexer* lexer, const char* message);
Token lexer_string(Lexer* lexer);
Token lexer_number(Lexer* lexer);
Token lexer_identifier(Lexer* lexer);

#endif // LEXER_H

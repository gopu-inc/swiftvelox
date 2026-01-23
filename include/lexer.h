#ifndef LEXER_H
#define LEXER_H

#include "common.h"

// Fonctions lexer seulement
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

#ifndef LEXER_H
#define LEXER_H

#include "common.h"

typedef struct {
    TokenType type;
    char* start;
    int length;
    int line;
    int col;
    union { 
        int64_t i; 
        double d; 
        char* s; 
        char c;
    };
} Token;

typedef struct {
    const char* start;
    const char* current;
    int line;
    int col;
} Scanner;

// Fonctions du lexer
void init_scanner(const char* source);
Token scan_token();
Token next_token();
int match(TokenType type);
void consume(TokenType type, const char* message);
void syntax_error(Token token, const char* message);

// Variables globales du lexer
extern Scanner scanner;
extern Token current_token;
extern Token previous_token;
extern int had_error;
extern int panic_mode;

#endif

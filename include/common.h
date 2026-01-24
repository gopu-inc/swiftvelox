#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>

// Définition de tous les types de tokens (TokenKind)
typedef enum {
   // Ajoute dans TokenKind :
    TK_IO_OPEN, TK_IO_CLOSE, TK_IO_READ, TK_IO_WRITE,
    TK_IO_SEEK, TK_IO_TELL, TK_IO_FLUSH,
    TK_IO_EXISTS, TK_IO_ISFILE, TK_IO_ISDIR,
    TK_IO_MKDIR, TK_IO_RMDIR, TK_IO_LISTDIR,
    TK_IO_REMOVE, TK_IO_RENAME, TK_IO_COPY,
    // Littéraux
    TK_INT_LIT, TK_FLOAT_LIT, TK_CHAR_LIT, TK_STR_LIT, TK_TRUE, TK_FALSE,
    // Opérateurs
    TK_PLUS, TK_MINUS, TK_MULT, TK_DIV, TK_MOD, 
    TK_ASSIGN, TK_EQ, TK_NEQ, TK_GT, TK_LT, TK_GTE, TK_LTE,
    TK_AND, TK_OR, TK_NOT, TK_BIT_AND, TK_BIT_OR,
    // Ponctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE, TK_COLON, TK_COMMA, TK_PERIOD,
    // Mots-clés (Keywords)
    TK_LET, TK_IF, TK_ELSE, TK_WHILE, TK_FOR, TK_IN, TK_END, TK_FUN, 
    TK_STRUCT, TK_RET, TK_IMPORT, TK_EXTERN, TK_CAST, TK_TYPE_OF,
    // Types
    TK_TYPE_INT, TK_TYPE_BOOL, TK_TYPE_VOID, TK_TYPE_STR,
    // Spécial
    TK_IDENT, TK_EOF, TK_ERROR
} TokenKind;

// Structure d'un Token
typedef struct {
    TokenKind kind;
    char* start;     // Pointeur vers le début du texte dans la source
    int length;      // Longueur du texte
    int line;        // Pour le débogage
} Token;

// Table de correspondance pour les mots-clés
typedef struct {
    const char* kw;
    TokenKind kind;
} Keyword;

static const Keyword keywords[] = {
    {"let", TK_LET}, {"if", TK_IF}, {"else", TK_ELSE},
    {"while", TK_WHILE}, {"for", TK_FOR}, {"in", TK_IN},
    {"end", TK_END}, {"fun", TK_FUN}, {"struct", TK_STRUCT},
    {"ret", TK_RET}, {"import", TK_IMPORT}, {"extern", TK_EXTERN},
    {"cast", TK_CAST}, {"true", TK_TRUE}, {"false", TK_FALSE},
    {"int", TK_TYPE_INT}, {"bool", TK_TYPE_BOOL}, {"void", TK_TYPE_VOID},
    {NULL, TK_ERROR}
};

#endif

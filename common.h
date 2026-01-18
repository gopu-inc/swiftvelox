#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>     // Pour access()
#include <sys/stat.h>   // Pour stat()

// ======================================================
// [SECTION] COULEURS POUR LE TERMINAL
// ======================================================
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define RESET   "\033[0m"

// ======================================================
// [SECTION] TOKEN DEFINITIONS
// ======================================================
typedef enum {
    // Literals
    TK_INT, TK_FLOAT, TK_STRING, TK_TRUE, TK_FALSE,
    // Identifiers
    TK_IDENT,
    // Operators
    TK_PLUS, TK_MINUS, TK_MULT, TK_DIV, TK_MOD,
    TK_ASSIGN, TK_EQ, TK_NEQ, TK_GT, TK_LT, TK_GTE, TK_LTE,
    TK_AND, TK_OR, TK_NOT,
    // Punctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_COMMA, TK_SEMICOLON, TK_COLON, TK_PERIOD,
    // Keywords
    TK_VAR, TK_CONST, TK_PRINT, TK_IF, TK_ELSE,
    TK_WHILE, TK_FOR, TK_FUNC, TK_RETURN, TK_IMPORT,
    // Type keywords
    TK_TYPE_INT, TK_TYPE_FLOAT, TK_TYPE_STR, TK_TYPE_BOOL,
    // Special
    TK_EOF, TK_ERROR
} TokenKind;

// Token structure
typedef struct {
    TokenKind kind;
    const char* start;
    int length;
    int line;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
    } value;
} Token;

// Keyword mapping
typedef struct {
    const char* keyword;
    TokenKind kind;
} Keyword;

static const Keyword keywords[] = {
    // Control flow
    {"if", TK_IF},
    {"else", TK_ELSE},
    {"while", TK_WHILE},
    {"for", TK_FOR},
    // Declarations
    {"var", TK_VAR},
    {"const", TK_CONST},
    // Functions
    {"func", TK_FUNC},
    {"return", TK_RETURN},
    // I/O
    {"print", TK_PRINT},
    // Modules
    {"import", TK_IMPORT},
    // Literals
    {"true", TK_TRUE},
    {"false", TK_FALSE},
    // Types
    {"int", TK_TYPE_INT},
    {"float", TK_TYPE_FLOAT},
    {"string", TK_TYPE_STR},
    {"bool", TK_TYPE_BOOL},
    // Mots-clés supplémentaires
    {"from", TK_IDENT},
    // End marker
    {NULL, TK_ERROR}
};

// ======================================================
// [SECTION] AST NODE DEFINITIONS
// ======================================================
typedef enum {
    // Expressions
    NODE_INT,
    NODE_FLOAT,
    NODE_STRING,
    NODE_IDENT,
    NODE_BINARY,
    // Statements
    NODE_VAR,
    NODE_ASSIGN,
    NODE_PRINT,
    NODE_IMPORT,
    NODE_BLOCK
} NodeType;

typedef struct ASTNode {
    NodeType type;
    struct ASTNode* left;
    struct ASTNode* right;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        char* name;
        char** imports;
    } data;
    int import_count;
    char* from_module;
} ASTNode;

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static inline char* str_copy(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* dest = malloc(len + 1);
    if (dest) strcpy(dest, src);
    return dest;
}

static inline char* str_ncopy(const char* src, int n) {
    if (!src || n <= 0) return NULL;
    char* dest = malloc(n + 1);
    if (dest) {
        strncpy(dest, src, n);
        dest[n] = '\0';
    }
    return dest;
}

#endif

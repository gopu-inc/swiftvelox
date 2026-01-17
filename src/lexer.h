#ifndef LEXER_H
#define LEXER_H

#include "value.h"

// ===== LEXER =====
typedef enum {
    // Keywords
    TK_MODULE, TK_IMPORT, TK_EXPORT, TK_FROM, TK_AS,
    TK_FN, TK_LET, TK_CONST, TK_VAR, TK_MUT,
    TK_IF, TK_ELSE, TK_ELIF, TK_UNLESS, TK_WHEN,
    TK_FOR, TK_IN, TK_WHILE, TK_DO, TK_REPEAT, TK_UNTIL,
    TK_MATCH, TK_CASE, TK_DEFAULT, TK_WHERE,
    TK_TRY, TK_CATCH, TK_FINALLY, TK_THROW,
    TK_RETURN, TK_YIELD, TK_BREAK, TK_CONTINUE, TK_SKIP,
    TK_TRUE, TK_FALSE, TK_NIL, TK_UNDEFINED,
    TK_CLASS, TK_STRUCT, TK_ENUM, TK_TRAIT, TK_INTERFACE,
    TK_EXTENDS, TK_IMPLEMENTS, TK_NEW, TK_DELETE,
    TK_PUBLIC, TK_PRIVATE, TK_PROTECTED, TK_STATIC,
    TK_ABSTRACT, TK_FINAL, TK_OVERRIDE,
    TK_ASYNC, TK_AWAIT, TK_ASYNC_AWAIT,
    TK_TYPEOF, TK_INSTANCEOF, TK_VOID,
    TK_NAMESPACE, TK_USING, TK_WITH,
    TK_DEFMACRO, TK_MACRO, TK_QUOTE, TK_UNQUOTE,
    TK_DECORATOR, TK_ANNOTATION,
    TK_ASSERT, TK_TEST, TK_SUITE, TK_BENCH,
    TK_DEBUGGER, TK_TRACER, TK_LOG,
    TK_SUPER, TK_THIS, TK_CONSTRUCTOR,
    
    // Types
    TK_INT_TYPE, TK_FLOAT_TYPE, TK_STRING_TYPE, TK_BOOL_TYPE,
    TK_CHAR_TYPE, TK_ARRAY_TYPE, TK_OBJECT_TYPE, TK_FUNC_TYPE,
    TK_ANY_TYPE, TK_VOID_TYPE, TK_NULLABLE_TYPE,
    
    // Literals
    TK_IDENTIFIER, TK_NUMBER, TK_STRING_LIT, TK_CHAR_LIT,
    TK_REGEX_LIT, TK_TEMPLATE_LIT,
    
    // Operators
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_PLUS_EQ, TK_MINUS_EQ, TK_STAR_EQ, TK_SLASH_EQ,
    TK_PLUS_PLUS, TK_MINUS_MINUS,
    TK_PLUS_EQ_EQ, TK_MINUS_EQ_EQ,
    TK_EQ, TK_EQEQ, TK_EQEQEQ, TK_BANGEQ, TK_BANGEQEQ,
    TK_LT, TK_GT, TK_LTEQ, TK_GTEQ,
    TK_LSHIFT, TK_RSHIFT, TK_URSHIFT,
    TK_AMPERSAND, TK_BAR, TK_CARET, TK_TILDE,
    TK_AMPERSAND_EQ, TK_BAR_EQ, TK_CARET_EQ,
    TK_LSHIFT_EQ, TK_RSHIFT_EQ, TK_URSHIFT_EQ,
    TK_AND, TK_OR, TK_NOT, TK_XOR,
    TK_QUESTION, TK_DOUBLE_QUESTION, TK_BANG, TK_DOUBLE_BANG,
    TK_DOT, TK_DOTDOT, TK_DOTDOTDOT,
    TK_COLON, TK_DOUBLE_COLON, TK_SEMICOLON, TK_COMMA,
    TK_ARROW, TK_FAT_ARROW, TK_DOUBLE_ARROW,
    TK_PIPE, TK_DOUBLE_PIPE, TK_PIPELINE,
    TK_AT, TK_HASH, TK_DOLLAR,
    
    // Grouping
    TK_LPAREN, TK_RPAREN, TK_LBRACKET, TK_RBRACKET,
    TK_LBRACE, TK_RBRACE, TK_LANGLE, TK_RANGLE,
    
    // Special
    TK_UNDERSCORE, TK_ELLIPSIS, TK_BACKTICK,
    TK_SHEBANG, TK_COMMENT, TK_MULTILINE_COMMENT,
    TK_EOF, TK_ERROR
} TokenType;

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

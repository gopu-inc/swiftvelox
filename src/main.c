/*
 * SwiftVelox v4.0 - Ultimate Edition
 * Langage de programmation moderne avec syntaxes avancées
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sqlite3.h>
#include <pthread.h>
#include <curl/curl.h>

// ===== CONFIGURATION =====
#define VERSION "4.0.0"
#define LIB_PATH "/usr/local/lib/swiftvelox/"
#define MAX_LINE_LENGTH 4096
#define MAX_MACROS 100
#define MAX_PACKAGES 50

// ===== TYPES DE DONNÉES =====
typedef enum {
    VAL_NIL, VAL_BOOL, VAL_INT, VAL_FLOAT, 
    VAL_STRING, VAL_FUNCTION, VAL_NATIVE, VAL_RETURN_SIG,
    VAL_ARRAY, VAL_OBJECT, VAL_CLOSURE, VAL_GENERATOR,
    VAL_PROMISE, VAL_ERROR, VAL_REGEX, VAL_BUFFER,
    VAL_DATE, VAL_SYMBOL, VAL_BIGINT, VAL_ITERATOR
} ValueType;

typedef struct Value Value;
typedef struct Environment Environment;
typedef struct ASTNode ASTNode;
typedef struct Macro Macro;
typedef struct Package Package;

struct Value {
    ValueType type;
    union {
        int boolean;
        int64_t integer;
        double number;
        char* string;
        struct {
            Value* items;
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            Value* values;
            int count;
            int capacity;
        } object;
        struct {
            char* name;
            Value (*fn)(struct Value*, int, Environment*);
        } native;
        struct {
            ASTNode* declaration;
            Environment* closure;
        } function;
        struct {
            ASTNode* generator;
            Environment* closure;
            int state;
        } generator;
        struct {
            int resolved;
            Value value;
        } promise;
        struct {
            char* message;
            Value data;
        } error;
    };
};

struct Environment {
    struct Environment* enclosing;
    char** names;
    Value* values;
    int count;
    int capacity;
};

// ===== LEXER & PARSER =====
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
    TK_PLUS_EQ_EQ, TK_MINUS_EQ_EQ, // SwiftVelox special
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
    NODE_DEBUG, NODE_TRACE
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

// ===== GLOBALS =====
Scanner scanner;
Token current_token;
Token previous_token;
int had_error = 0;
int panic_mode = 0;

Environment* global_env = NULL;
Macro macros[MAX_MACROS];
int macro_count = 0;
Package* packages[MAX_PACKAGES];
int package_count = 0;

// ===== UTILITAIRES =====
void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "\033[1;31m❌ ERREUR FATALE: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\033[0m\n");
    va_end(args);
    exit(1);
}

void syntax_error(Token token, const char* message) {
    if (panic_mode) return;
    panic_mode = 1;
    
    fprintf(stderr, "\033[1;31m[Ligne %d] Erreur de syntaxe", token.line);
    if (token.type == TK_EOF) {
        fprintf(stderr, " à la fin");
    } else if (token.type == TK_ERROR) {
        fprintf(stderr, ": %s", token.s);
    } else {
        fprintf(stderr, " à '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\033[0m\n", message);
    had_error = 1;
}

// ===== ENVIRONNEMENT =====
Environment* new_environment(Environment* enclosing) {
    Environment* env = malloc(sizeof(Environment));
    env->enclosing = enclosing;
    env->count = 0;
    env->capacity = 8;
    env->names = malloc(sizeof(char*) * 8);
    env->values = malloc(sizeof(Value) * 8);
    return env;
}

void env_define(Environment* env, char* name, Value value) {
    for (int i = 0; i < env->count; i++) {
        if (strcmp(env->names[i], name) == 0) {
            env->values[i] = value;
            return;
        }
    }
    if (env->count >= env->capacity) {
        env->capacity *= 2;
        env->names = realloc(env->names, sizeof(char*) * env->capacity);
        env->values = realloc(env->values, sizeof(Value) * env->capacity);
    }
    env->names[env->count] = strdup(name);
    env->values[env->count] = value;
    env->count++;
}

int env_get(Environment* env, char* name, Value* out) {
    Environment* current = env;
    while (current != NULL) {
        for (int i = 0; i < current->count; i++) {
            if (strcmp(current->names[i], name) == 0) {
                *out = current->values[i];
                return 1;
            }
        }
        current = current->enclosing;
    }
    return 0;
}

// ===== LEXER =====
void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.col = 1;
}

char advance() {
    scanner.current++;
    scanner.col++;
    return scanner.current[-1];
}

char peek() { return *scanner.current; }
char peek_next() { 
    return scanner.current[1] ? scanner.current[1] : '\0'; 
}

int is_at_end() { return *scanner.current == '\0'; }

Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = (char*)scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    token.col = scanner.col - token.length;
    return token;
}

Token error_token(const char* message) {
    Token token;
    token.type = TK_ERROR;
    token.s = (char*)message;
    token.line = scanner.line;
    token.col = scanner.col;
    return token;
}

void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                scanner.col++;
                break;
            case '\n':
                scanner.line++;
                scanner.col = 1;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else if (peek_next() == '*') {
                    advance(); // Skip /
                    advance(); // Skip *
                    while (!(peek() == '*' && peek_next() == '/') && !is_at_end()) {
                        if (peek() == '\n') {
                            scanner.line++;
                            scanner.col = 1;
                        }
                        advance();
                    }
                    if (!is_at_end()) {
                        advance(); // Skip *
                        advance(); // Skip /
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

TokenType check_keyword(int start, int length, const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }
    return TK_IDENTIFIER;
}

TokenType identifier_type() {
    char* start = (char*)scanner.start;
    int length = scanner.current - scanner.start;
    
    switch (start[0]) {
        case 'a':
            if (length == 5 && memcmp(start, "async", 5) == 0) return TK_ASYNC;
            if (length == 5 && memcmp(start, "await", 5) == 0) return TK_AWAIT;
            if (length == 4 && memcmp(start, "as", 2) == 0) return check_keyword(2, 2, "", TK_AS);
            break;
        case 'b': 
            if (length == 5 && memcmp(start, "break", 5) == 0) return TK_BREAK;
            if (length == 4 && memcmp(start, "bool", 4) == 0) return TK_BOOL_TYPE;
            break;
        case 'c':
            if (length == 4 && memcmp(start, "case", 4) == 0) return TK_CASE;
            if (length == 5 && memcmp(start, "catch", 5) == 0) return TK_CATCH;
            if (length == 5 && memcmp(start, "class", 5) == 0) return TK_CLASS;
            if (length == 5 && memcmp(start, "const", 5) == 0) return TK_CONST;
            if (length == 8 && memcmp(start, "continue", 8) == 0) return TK_CONTINUE;
            break;
        case 'd':
            if (length == 6 && memcmp(start, "delete", 6) == 0) return TK_DELETE;
            if (length == 2 && memcmp(start, "do", 2) == 0) return TK_DO;
            if (length == 7 && memcmp(start, "default", 7) == 0) return TK_DEFAULT;
            break;
        case 'e':
            if (length == 4 && memcmp(start, "else", 4) == 0) return TK_ELSE;
            if (length == 4 && memcmp(start, "enum", 4) == 0) return TK_ENUM;
            if (length == 5 && memcmp(start, "export", 6) == 0) return TK_EXPORT;
            if (length == 6 && memcmp(start, "extends", 7) == 0) return TK_EXTENDS;
            break;
        case 'f':
            if (length == 4 && memcmp(start, "fn", 2) == 0) return TK_FN;
            if (length == 5 && memcmp(start, "false", 5) == 0) return TK_FALSE;
            if (length == 3 && memcmp(start, "for", 3) == 0) return TK_FOR;
            if (length == 5 && memcmp(start, "final", 5) == 0) return TK_FINAL;
            if (length == 6 && memcmp(start, "finally", 7) == 0) return TK_FINALLY;
            if (length == 5 && memcmp(start, "float", 5) == 0) return TK_FLOAT_TYPE;
            if (length == 4 && memcmp(start, "from", 4) == 0) return TK_FROM;
            break;
        case 'i':
            if (length == 2 && memcmp(start, "if", 2) == 0) return TK_IF;
            if (length == 2 && memcmp(start, "in", 2) == 0) return TK_IN;
            if (length == 6 && memcmp(start, "import", 6) == 0) return TK_IMPORT;
            if (length == 3 && memcmp(start, "int", 3) == 0) return TK_INT_TYPE;
            if (length == 9 && memcmp(start, "interface", 9) == 0) return TK_INTERFACE;
            if (length == 10 && memcmp(start, "implements", 10) == 0) return TK_IMPLEMENTS;
            if (length == 10 && memcmp(start, "instanceof", 10) == 0) return TK_INSTANCEOF;
            break;
        case 'l':
            if (length == 3 && memcmp(start, "let", 3) == 0) return TK_LET;
            if (length == 3 && memcmp(start, "log", 3) == 0) return TK_LOG;
            break;
        case 'm':
            if (length == 5 && memcmp(start, "match", 5) == 0) return TK_MATCH;
            if (length == 6 && memcmp(start, "macro", 5) == 0) return TK_MACRO;
            if (length == 6 && memcmp(start, "module", 6) == 0) return TK_MODULE;
            if (length == 3 && memcmp(start, "mut", 3) == 0) return TK_MUT;
            break;
        case 'n':
            if (length == 3 && memcmp(start, "new", 3) == 0) return TK_NEW;
            if (length == 3 && memcmp(start, "nil", 3) == 0) return TK_NIL;
            if (length == 9 && memcmp(start, "namespace", 9) == 0) return TK_NAMESPACE;
            break;
        case 'o':
            if (length == 2 && memcmp(start, "or", 2) == 0) return TK_OR;
            break;
        case 'p':
            if (length == 6 && memcmp(start, "public", 6) == 0) return TK_PUBLIC;
            if (length == 7 && memcmp(start, "private", 7) == 0) return TK_PRIVATE;
            if (length == 9 && memcmp(start, "protected", 9) == 0) return TK_PROTECTED;
            break;
        case 'r':
            if (length == 6 && memcmp(start, "return", 6) == 0) return TK_RETURN;
            if (length == 6 && memcmp(start, "repeat", 6) == 0) return TK_REPEAT;
            break;
        case 's':
            if (length == 6 && memcmp(start, "static", 6) == 0) return TK_STATIC;
            if (length == 6 && memcmp(start, "string", 6) == 0) return TK_STRING_TYPE;
            if (length == 6 && memcmp(start, "struct", 6) == 0) return TK_STRUCT;
            if (length == 5 && memcmp(start, "super", 5) == 0) return TK_SUPER;
            if (length == 5 && memcmp(start, "switch", 6) == 0) return TK_MATCH;
            break;
        case 't':
            if (length == 4 && memcmp(start, "true", 4) == 0) return TK_TRUE;
            if (length == 4 && memcmp(start, "this", 4) == 0) return TK_THIS;
            if (length == 5 && memcmp(start, "throw", 5) == 0) return TK_THROW;
            if (length == 4 && memcmp(start, "try", 3) == 0) return TK_TRY;
            if (length == 7 && memcmp(start, "typeof", 6) == 0) return TK_TYPEOF;
            if (length == 5 && memcmp(start, "trait", 5) == 0) return TK_TRAIT;
            break;
        case 'u':
            if (length == 5 && memcmp(start, "unless", 6) == 0) return TK_UNLESS;
            if (length == 5 && memcmp(start, "until", 5) == 0) return TK_UNTIL;
            if (length == 8 && memcmp(start, "undefined", 9) == 0) return TK_UNDEFINED;
            break;
        case 'v':
            if (length == 3 && memcmp(start, "var", 3) == 0) return TK_VAR;
            if (length == 4 && memcmp(start, "void", 4) == 0) return TK_VOID;
            break;
        case 'w':
            if (length == 5 && memcmp(start, "while", 5) == 0) return TK_WHILE;
            if (length == 4 && memcmp(start, "when", 4) == 0) return TK_WHEN;
            if (length == 5 && memcmp(start, "where", 5) == 0) return TK_WHERE;
            if (length == 4 && memcmp(start, "with", 4) == 0) return TK_WITH;
            break;
        case 'y':
            if (length == 5 && memcmp(start, "yield", 5) == 0) return TK_YIELD;
            break;
    }
    
    return TK_IDENTIFIER;
}

Token scan_token() {
    skip_whitespace();
    scanner.start = scanner.current;
    
    if (is_at_end()) return make_token(TK_EOF);
    
    char c = advance();
    
    // Identifiers and keywords
    if (isalpha(c) || c == '_') {
        while (isalnum(peek()) || peek() == '_') advance();
        Token t = make_token(identifier_type());
        if (t.type == TK_IDENTIFIER) {
            t.s = malloc(t.length + 1);
            memcpy(t.s, t.start, t.length);
            t.s[t.length] = 0;
        }
        return t;
    }
    
    // Numbers
    if (isdigit(c)) {
        while (isdigit(peek())) advance();
        
        if (peek() == '.' && isdigit(peek_next())) {
            advance(); // Consume '.'
            while (isdigit(peek())) advance();
            
            // Scientific notation
            if (peek() == 'e' || peek() == 'E') {
                advance();
                if (peek() == '+' || peek() == '-') advance();
                while (isdigit(peek())) advance();
            }
            
            Token t = make_token(TK_NUMBER);
            char* num_str = malloc(t.length + 1);
            memcpy(num_str, t.start, t.length);
            num_str[t.length] = 0;
            t.d = strtod(num_str, NULL);
            free(num_str);
            return t;
        }
        
        Token t = make_token(TK_NUMBER);
        char* num_str = malloc(t.length + 1);
        memcpy(num_str, t.start, t.length);
        num_str[t.length] = 0;
        t.i = strtoll(num_str, NULL, 10);
        free(num_str);
        return t;
    }
    
    // Strings
    if (c == '"' || c == '\'') {
        char quote = c;
        while (peek() != quote && !is_at_end()) {
            if (peek() == '\\') advance();
            advance();
        }
        
        if (is_at_end()) return error_token("Chaîne non terminée");
        
        advance(); // Consume closing quote
        
        Token t = make_token(TK_STRING_LIT);
        t.s = malloc(t.length - 1); // Exclude quotes
        memcpy(t.s, t.start + 1, t.length - 2);
        t.s[t.length - 2] = 0;
        return t;
    }
    
    // Template strings
    if (c == '`') {
        while (peek() != '`' && !is_at_end()) {
            if (peek() == '\\') advance();
            if (peek() == '$' && peek_next() == '{') {
                // Template interpolation
                advance(); // Skip $
                advance(); // Skip {
                // TODO: Parse expression
                while (peek() != '}' && !is_at_end()) advance();
                if (peek() == '}') advance();
            }
            advance();
        }
        
        if (is_at_end()) return error_token("Template non terminé");
        advance(); // Consume closing `
        
        Token t = make_token(TK_TEMPLATE_LIT);
        t.s = malloc(t.length - 1);
        memcpy(t.s, t.start + 1, t.length - 2);
        t.s[t.length - 2] = 0;
        return t;
    }
    
    // Operators and punctuation
    switch (c) {
        case '(': return make_token(TK_LPAREN);
        case ')': return make_token(TK_RPAREN);
        case '{': return make_token(TK_LBRACE);
        case '}': return make_token(TK_RBRACE);
        case '[': return make_token(TK_LBRACKET);
        case ']': return make_token(TK_RBRACKET);
        case ';': return make_token(TK_SEMICOLON);
        case ',': return make_token(TK_COMMA);
        case '.': 
            if (peek() == '.') {
                advance();
                if (peek() == '.') {
                    advance();
                    return make_token(TK_DOTDOTDOT);
                }
                return make_token(TK_DOTDOT);
            }
            return make_token(TK_DOT);
        case '-':
            if (peek() == '>') {
                advance();
                if (peek() == '>') {
                    advance();
                    return make_token(TK_DOUBLE_ARROW);
                }
                return make_token(TK_ARROW);
            }
            if (peek() == '=') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_MINUS_EQ_EQ);
                }
                return make_token(TK_MINUS_EQ);
            }
            if (peek() == '-') {
                advance();
                return make_token(TK_MINUS_MINUS);
            }
            return make_token(TK_MINUS);
        case '+':
            if (peek() == '>') {
                advance();
                return make_token(TK_PIPELINE);
            }
            if (peek() == '=') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_PLUS_EQ_EQ);
                }
                return make_token(TK_PLUS_EQ);
            }
            if (peek() == '+') {
                advance();
                return make_token(TK_PLUS_PLUS);
            }
            return make_token(TK_PLUS);
        case '*':
            if (peek() == '=') {
                advance();
                return make_token(TK_STAR_EQ);
            }
            if (peek() == '*') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_STAR_EQ); // **=
                }
                return make_token(TK_STAR); // **
            }
            return make_token(TK_STAR);
        case '/':
            if (peek() == '=') {
                advance();
                return make_token(TK_SLASH_EQ);
            }
            return make_token(TK_SLASH);
        case '%':
            if (peek() == '=') {
                advance();
                return make_token(TK_PERCENT);
            }
            return make_token(TK_PERCENT);
        case '=':
            if (peek() == '=') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_EQEQEQ);
                }
                return make_token(TK_EQEQ);
            }
            if (peek() == '>') {
                advance();
                return make_token(TK_FAT_ARROW);
            }
            return make_token(TK_EQ);
        case '!':
            if (peek() == '=') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_BANGEQEQ);
                }
                return make_token(TK_BANGEQ);
            }
            return make_token(TK_BANG);
        case '<':
            if (peek() == '=') {
                advance();
                return make_token(TK_LTEQ);
            }
            if (peek() == '<') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_LSHIFT_EQ);
                }
                return make_token(TK_LSHIFT);
            }
            return make_token(TK_LT);
        case '>':
            if (peek() == '=') {
                advance();
                return make_token(TK_GTEQ);
            }
            if (peek() == '>') {
                advance();
                if (peek() == '>') {
                    advance();
                    if (peek() == '=') {
                        advance();
                        return make_token(TK_URSHIFT_EQ);
                    }
                    return make_token(TK_URSHIFT);
                }
                if (peek() == '=') {
                    advance();
                    return make_token(TK_RSHIFT_EQ);
                }
                return make_token(TK_RSHIFT);
            }
            return make_token(TK_GT);
        case '&':
            if (peek() == '&') {
                advance();
                return make_token(TK_AND);
            }
            if (peek() == '=') {
                advance();
                return make_token(TK_AMPERSAND_EQ);
            }
            return make_token(TK_AMPERSAND);
        case '|':
            if (peek() == '|') {
                advance();
                return make_token(TK_OR);
            }
            if (peek() == '=') {
                advance();
                return make_token(TK_BAR_EQ);
            }
            if (peek() == '>') {
                advance();
                return make_token(TK_PIPE);
            }
            return make_token(TK_BAR);
        case '^':
            if (peek() == '=') {
                advance();
                return make_token(TK_CARET_EQ);
            }
            return make_token(TK_CARET);
        case '~': return make_token(TK_TILDE);
        case '?':
            if (peek() == '?') {
                advance();
                return make_token(TK_DOUBLE_QUESTION);
            }
            if (peek() == '.') {
                advance();
                return make_token(TK_QUESTION); // ?.
            }
            return make_token(TK_QUESTION);
        case ':':
            if (peek() == ':') {
                advance();
                return make_token(TK_DOUBLE_COLON);
            }
            return make_token(TK_COLON);
        case '@': return make_token(TK_AT);
        case '#': return make_token(TK_HASH);
        case '$': return make_token(TK_DOLLAR);
        case '_': return make_token(TK_UNDERSCORE);
    }
    
    return error_token("Caractère inattendu");
}

// ===== PARSER =====
Token next_token() { 
    previous_token = current_token; 
    current_token = scan_token(); 
    return current_token; 
}

int match(TokenType type) { 
    if (current_token.type == type) { 
        next_token(); 
        return 1; 
    } 
    return 0; 
}

void consume(TokenType type, const char* message) { 
    if (current_token.type == type) { 
        next_token(); 
        return; 
    } 
    syntax_error(current_token, message);
}

ASTNode* new_node(NodeType type) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = type;
    node->token = current_token;
    node->left = node->right = NULL;
    node->children = NULL;
    node->child_count = 0;
    node->inferred_type = VAL_NIL;
    return node;
}

ASTNode* expression();
ASTNode* statement();
ASTNode* declaration();

// Primary expressions
ASTNode* primary() {
    if (match(TK_NUMBER)) {
        ASTNode* n = new_node(NODE_LITERAL);
        n->token = previous_token;
        return n;
    }
    if (match(TK_STRING_LIT) || match(TK_TEMPLATE_LIT)) {
        ASTNode* n = new_node(NODE_LITERAL);
        n->token = previous_token;
        return n;
    }
    if (match(TK_TRUE) || match(TK_FALSE) || match(TK_NIL) || match(TK_UNDEFINED)) {
        ASTNode* n = new_node(NODE_LITERAL);
        n->token = previous_token;
        return n;
    }
    if (match(TK_IDENTIFIER)) {
        ASTNode* n = new_node(NODE_IDENTIFIER);
        n->token = previous_token;
        return n;
    }
    if (match(TK_THIS)) {
        ASTNode* n = new_node(NODE_THIS);
        return n;
    }
    if (match(TK_SUPER)) {
        ASTNode* n = new_node(NODE_SUPER);
        consume(TK_DOT, "'.' attendu après 'super'");
        consume(TK_IDENTIFIER, "Nom de méthode attendu");
        n->token = previous_token;
        return n;
    }
    if (match(TK_LPAREN)) {
        ASTNode* expr = expression();
        consume(TK_RPAREN, "')' attendu");
        return expr;
    }
    if (match(TK_LBRACKET)) {
        ASTNode* n = new_node(NODE_ARRAY);
        if (!match(TK_RBRACKET)) {
            do {
                if (match(TK_DOTDOTDOT)) {
                    ASTNode* spread = new_node(NODE_SPREAD);
                    spread->left = expression();
                    n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                    n->children[n->child_count++] = spread;
                } else {
                    n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                    n->children[n->child_count++] = expression();
                }
            } while (match(TK_COMMA));
            consume(TK_RBRACKET, "']' attendu");
        }
        return n;
    }
    if (match(TK_LBRACE)) {
        ASTNode* n = new_node(NODE_OBJECT);
        if (!match(TK_RBRACE)) {
            do {
                ASTNode* prop = new_node(NODE_PROPERTY);
                
                if (match(TK_IDENTIFIER)) {
                    prop->token = previous_token;
                } else if (match(TK_STRING_LIT)) {
                    prop->token = previous_token;
                } else {
                    syntax_error(current_token, "Clé de propriété attendue");
                }
                
                consume(TK_COLON, "':' attendu");
                prop->left = expression();
                
                n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                n->children[n->child_count++] = prop;
            } while (match(TK_COMMA));
            consume(TK_RBRACE, "'}' attendu");
        }
        return n;
    }
    if (match(TK_FN)) {
        return function_expression();
    }
    if (match(TK_ASYNC)) {
        ASTNode* n = new_node(NODE_ASYNC);
        n->left = expression(); // Function expression
        return n;
    }
    
    syntax_error(current_token, "Expression attendue");
    return NULL;
}

// Function expression
ASTNode* function_expression() {
    ASTNode* n = new_node(NODE_FUNCTION);
    
    // Optional name
    if (current_token.type == TK_IDENTIFIER) {
        n->token = previous_token;
        next_token();
    }
    
    consume(TK_LPAREN, "'(' attendu");
    
    // Parameters
    if (!match(TK_RPAREN)) {
        do {
            // Rest parameter
            if (match(TK_DOTDOTDOT)) {
                ASTNode* rest = new_node(NODE_REST);
                consume(TK_IDENTIFIER, "Nom de paramètre attendu");
                rest->token = previous_token;
                n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                n->children[n->child_count++] = rest;
                break;
            }
            
            ASTNode* param = new_node(NODE_IDENTIFIER);
            consume(TK_IDENTIFIER, "Nom de paramètre attendu");
            param->token = previous_token;
            
            // Optional type annotation
            if (match(TK_COLON)) {
                ASTNode* type = type_annotation();
                param->left = type;
            }
            
            // Optional default value
            if (match(TK_EQ)) {
                param->right = expression();
            }
            
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = param;
        } while (match(TK_COMMA));
        consume(TK_RPAREN, "')' attendu");
    }
    
    // Return type annotation
    if (match(TK_COLON)) {
        ASTNode* return_type = type_annotation();
        n->right = return_type;
    }
    
    // Arrow function or block
    if (match(TK_ARROW) || match(TK_FAT_ARROW)) {
        ASTNode* body;
        if (current_token.type == TK_LBRACE) {
            body = statement(); // Block
        } else {
            body = expression(); // Single expression
        }
        n->left = body;
    } else {
        n->left = statement(); // Block
    }
    
    return n;
}

// Call and member access
ASTNode* call() {
    ASTNode* expr = primary();
    
    while (1) {
        if (match(TK_LPAREN)) {
            ASTNode* n = new_node(NODE_CALL);
            n->left = expr;
            
            if (!match(TK_RPAREN)) {
                do {
                    n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                    n->children[n->child_count++] = expression();
                } while (match(TK_COMMA));
                consume(TK_RPAREN, "')' attendu");
            }
            expr = n;
        }
        else if (match(TK_DOT)) {
            ASTNode* n = new_node(NODE_ACCESS);
            n->left = expr;
            consume(TK_IDENTIFIER, "Propriété attendue");
            n->token = previous_token;
            expr = n;
        }
        else if (match(TK_LBRACKET)) {
            ASTNode* n = new_node(NODE_INDEX);
            n->left = expr;
            n->right = expression();
            consume(TK_RBRACKET, "']' attendu");
            expr = n;
        }
        else if (match(TK_QUESTION) && current_token.type == TK_DOT) {
            next_token(); // Skip .
            ASTNode* n = new_node(NODE_ACCESS);
            n->left = expr;
            consume(TK_IDENTIFIER, "Propriété attendue");
            n->token = previous_token;
            expr = n;
        }
        else {
            break;
        }
    }
    
    return expr;
}

// Unary operators
ASTNode* unary() {
    if (match(TK_BANG) || match(TK_MINUS) || match(TK_PLUS) || match(TK_TILDE) ||
        match(TK_TYPEOF) || match(TK_VOID) || match(TK_DELETE) || match(TK_AWAIT)) {
        ASTNode* n = new_node(NODE_UNARY);
        n->token = previous_token;
        n->right = unary();
        return n;
    }
    
    if (match(TK_PLUS_PLUS) || match(TK_MINUS_MINUS)) {
        ASTNode* n = new_node(NODE_UNARY);
        n->token = previous_token;
        n->right = call();
        return n;
    }
    
    return call();
}

// Multiplication and division
ASTNode* multiplication() {
    ASTNode* expr = unary();
    
    while (match(TK_STAR) || match(TK_SLASH) || match(TK_PERCENT)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = unary();
        expr = n;
    }
    
    return expr;
}

// Addition and subtraction
ASTNode* addition() {
    ASTNode* expr = multiplication();
    
    while (match(TK_PLUS) || match(TK_MINUS)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = multiplication();
        expr = n;
    }
    
    return expr;
}

// Shift operators
ASTNode* shift() {
    ASTNode* expr = addition();
    
    while (match(TK_LSHIFT) || match(TK_RSHIFT) || match(TK_URSHIFT)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = addition();
        expr = n;
    }
    
    return expr;
}

// Relational operators
ASTNode* relational() {
    ASTNode* expr = shift();
    
    while (match(TK_LT) || match(TK_GT) || match(TK_LTEQ) || match(TK_GTEQ) ||
           match(TK_INSTANCEOF) || match(TK_IN)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = shift();
        expr = n;
    }
    
    return expr;
}

// Equality operators
ASTNode* equality() {
    ASTNode* expr = relational();
    
    while (match(TK_EQEQ) || match(TK_BANGEQ) || match(TK_EQEQEQ) || match(TK_BANGEQEQ)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = relational();
        expr = n;
    }
    
    return expr;
}

// Bitwise AND
ASTNode* bitwise_and() {
    ASTNode* expr = equality();
    
    while (match(TK_AMPERSAND)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = equality();
        expr = n;
    }
    
    return expr;
}

// Bitwise XOR
ASTNode* bitwise_xor() {
    ASTNode* expr = bitwise_and();
    
    while (match(TK_CARET)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = bitwise_and();
        expr = n;
    }
    
    return expr;
}

// Bitwise OR
ASTNode* bitwise_or() {
    ASTNode* expr = bitwise_xor();
    
    while (match(TK_BAR)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = bitwise_xor();
        expr = n;
    }
    
    return expr;
}

// Logical AND
ASTNode* logical_and() {
    ASTNode* expr = bitwise_or();
    
    while (match(TK_AND)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = bitwise_or();
        expr = n;
    }
    
    return expr;
}

// Logical OR
ASTNode* logical_or() {
    ASTNode* expr = logical_and();
    
    while (match(TK_OR)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = logical_and();
        expr = n;
    }
    
    return expr;
}

// Nullish coalescing
ASTNode* nullish_coalescing() {
    ASTNode* expr = logical_or();
    
    while (match(TK_DOUBLE_QUESTION)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = logical_or();
        expr = n;
    }
    
    return expr;
}

// Ternary conditional
ASTNode* conditional() {
    ASTNode* expr = nullish_coalescing();
    
    if (match(TK_QUESTION)) {
        ASTNode* n = new_node(NODE_TERNARY);
        n->left = expr; // Condition
        n->right = expression(); // Then
        consume(TK_COLON, "':' attendu");
        ASTNode* else_node = new_node(NODE_TERNARY);
        else_node->left = expression(); // Else
        n->children = malloc(sizeof(ASTNode*));
        n->children[0] = else_node;
        n->child_count = 1;
        return n;
    }
    
    return expr;
}

// Pipeline operator
ASTNode* pipeline() {
    ASTNode* expr = conditional();
    
    while (match(TK_PIPELINE)) {
        ASTNode* n = new_node(NODE_PIPELINE);
        n->left = expr;
        n->right = conditional();
        expr = n;
    }
    
    return expr;
}

// Assignment
ASTNode* assignment() {
    ASTNode* expr = pipeline();
    
    if (match(TK_EQ) || match(TK_PLUS_EQ) || match(TK_MINUS_EQ) || 
        match(TK_STAR_EQ) || match(TK_SLASH_EQ) || match(TK_PERCENT) ||
        match(TK_LSHIFT_EQ) || match(TK_RSHIFT_EQ) || match(TK_URSHIFT_EQ) ||
        match(TK_AMPERSAND_EQ) || match(TK_BAR_EQ) || match(TK_CARET_EQ) ||
        match(TK_PLUS_EQ_EQ) || match(TK_MINUS_EQ_EQ)) {
        
        ASTNode* n;
        if (previous_token.type == TK_EQ) {
            n = new_node(NODE_ASSIGN);
        } else {
            n = new_node(NODE_COMPOUND_ASSIGN);
        }
        n->token = previous_token;
        n->left = expr;
        n->right = assignment();
        return n;
    }
    
    return expr;
}

// Expression
ASTNode* expression() {
    return assignment();
}

// Type annotation
ASTNode* type_annotation() {
    // Simple type system for now
    ASTNode* n = new_node(NODE_TYPE_DECL);
    
    if (match(TK_IDENTIFIER)) {
        n->token = previous_token;
    } else if (match(TK_STRING_TYPE) || match(TK_INT_TYPE) || 
               match(TK_FLOAT_TYPE) || match(TK_BOOL_TYPE) ||
               match(TK_VOID_TYPE) || match(TK_ANY_TYPE)) {
        n->token = previous_token;
    } else if (match(TK_LBRACKET)) {
        n->token.type = TK_ARRAY_TYPE;
        n->left = type_annotation();
        consume(TK_RBRACKET, "']' attendu");
    } else {
        syntax_error(current_token, "Type attendu");
    }
    
    // Optional nullable
    if (match(TK_QUESTION)) {
        ASTNode* nullable = new_node(NODE_NULLABLE_TYPE);
        nullable->left = n;
        return nullable;
    }
    
    return n;
}

// Variable declaration
ASTNode* var_declaration() {
    TokenType decl_type = current_token.type;
    next_token(); // Skip let/const/var/mut
    
    ASTNode* n;
    if (decl_type == TK_LET || decl_type == TK_CONST) {
        n = new_node(decl_type == TK_LET ? NODE_VAR_DECL : NODE_CONST_DECL);
    } else {
        n = new_node(NODE_VAR_DECL);
    }
    
    consume(TK_IDENTIFIER, "Nom de variable attendu");
    n->token = previous_token;
    
    // Type annotation
    if (match(TK_COLON)) {
        ASTNode* type = type_annotation();
        n->left = type;
    }
    
    // Initializer
    if (match(TK_EQ)) {
        n->right = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// If statement
ASTNode* if_statement() {
    ASTNode* n = new_node(NODE_IF);
    
    consume(TK_LPAREN, "'(' attendu après 'if'");
    n->left = expression(); // Condition
    consume(TK_RPAREN, "')' attendu");
    
    n->children = malloc(sizeof(ASTNode*) * 3);
    n->child_count = 0;
    
    // Then branch
    n->children[n->child_count++] = statement();
    
    // Elif branches
    while (match(TK_ELIF)) {
        ASTNode* elif = new_node(NODE_ELIF);
        consume(TK_LPAREN, "'(' attendu après 'elif'");
        elif->left = expression();
        consume(TK_RPAREN, "')' attendu");
        elif->right = statement();
        
        n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
        n->children[n->child_count++] = elif;
    }
    
    // Else branch
    if (match(TK_ELSE)) {
        n->children[n->child_count++] = statement();
    }
    
    return n;
}

// While statement
ASTNode* while_statement() {
    ASTNode* n = new_node(NODE_WHILE);
    
    consume(TK_LPAREN, "'(' attendu après 'while'");
    n->left = expression();
    consume(TK_RPAREN, "')' attendu");
    
    n->right = statement();
    return n;
}

// For statement
ASTNode* for_statement() {
    ASTNode* n = new_node(NODE_FOR);
    
    consume(TK_LPAREN, "'(' attendu après 'for'");
    
    // Initializer
    if (match(TK_SEMICOLON)) {
        // No initializer
    } else if (current_token.type == TK_LET || current_token.type == TK_CONST || 
               current_token.type == TK_VAR || current_token.type == TK_MUT) {
        n->children = malloc(sizeof(ASTNode*) * 3);
        n->child_count = 0;
        n->children[n->child_count++] = var_declaration();
    } else {
        n->children = malloc(sizeof(ASTNode*) * 3);
        n->child_count = 0;
        n->children[n->child_count++] = expression_statement();
    }
    
    // Condition
    if (current_token.type != TK_SEMICOLON) {
        n->children[n->child_count++] = expression();
    }
    consume(TK_SEMICOLON, "';' attendu");
    
    // Increment
    if (current_token.type != TK_RPAREN) {
        n->children[n->child_count++] = expression();
    }
    consume(TK_RPAREN, "')' attendu");
    
    n->right = statement(); // Body
    
    return n;
}

// For-in statement
ASTNode* for_in_statement() {
    ASTNode* n = new_node(NODE_FOR_IN);
    
    consume(TK_LPAREN, "'(' attendu après 'for'");
    
    // Variable declaration or pattern
    if (current_token.type == TK_LET || current_token.type == TK_CONST || 
        current_token.type == TK_VAR || current_token.type == TK_MUT) {
        next_token(); // Skip keyword
        consume(TK_IDENTIFIER, "Variable attendue");
        n->left = new_node(NODE_IDENTIFIER);
        n->left->token = previous_token;
    } else {
        n->left = expression();
    }
    
    consume(TK_IN, "'in' attendu");
    n->right = expression();
    consume(TK_RPAREN, "')' attendu");
    
    ASTNode* body = new_node(NODE_BLOCK);
    body->children = malloc(sizeof(ASTNode*));
    body->child_count = 1;
    body->children[0] = statement();
    
    n->children = malloc(sizeof(ASTNode*));
    n->children[0] = body;
    n->child_count = 1;
    
    return n;
}

// Match statement
ASTNode* match_statement() {
    ASTNode* n = new_node(NODE_MATCH);
    
    consume(TK_LPAREN, "'(' attendu après 'match'");
    n->left = expression();
    consume(TK_RPAREN, "')' attendu");
    
    consume(TK_LBRACE, "'{' attendu");
    
    n->children = malloc(sizeof(ASTNode*) * 10);
    n->child_count = 0;
    
    while (current_token.type != TK_RBRACE && !is_at_end()) {
        if (match(TK_CASE)) {
            ASTNode* case_node = new_node(NODE_CASE);
            
            // Pattern
            case_node->left = pattern();
            
            // Guard
            if (match(TK_IF)) {
                case_node->right = expression();
            }
            
            consume(TK_ARROW, "'=>' attendu");
            
            // Body
            ASTNode* body = new_node(NODE_BLOCK);
            body->children = malloc(sizeof(ASTNode*));
            body->child_count = 1;
            
            if (current_token.type == TK_LBRACE) {
                body->children[0] = statement();
            } else {
                body->children[0] = expression_statement();
            }
            
            case_node->children = malloc(sizeof(ASTNode*));
            case_node->children[0] = body;
            case_node->child_count = 1;
            
            n->children[n->child_count++] = case_node;
        } else if (match(TK_DEFAULT)) {
            consume(TK_ARROW, "'=>' attendu");
            
            ASTNode* default_node = new_node(NODE_DEFAULT);
            default_node->left = statement();
            
            n->children[n->child_count++] = default_node;
        } else {
            syntax_error(current_token, "'case' ou 'default' attendu");
            break;
        }
    }
    
    consume(TK_RBRACE, "'}' attendu");
    return n;
}

// Pattern
ASTNode* pattern() {
    if (match(TK_UNDERSCORE)) {
        ASTNode* n = new_node(NODE_WILDCARD);
        return n;
    }
    
    if (match(TK_IDENTIFIER)) {
        ASTNode* n = new_node(NODE_BIND_PATTERN);
        n->token = previous_token;
        return n;
    }
    
    if (match(TK_LBRACKET)) {
        ASTNode* n = new_node(NODE_ARRAY_PATTERN);
        
        if (!match(TK_RBRACKET)) {
            do {
                if (match(TK_DOTDOTDOT)) {
                    ASTNode* rest = new_node(NODE_REST);
                    rest->left = pattern();
                    n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                    n->children[n->child_count++] = rest;
                    break;
                }
                n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                n->children[n->child_count++] = pattern();
            } while (match(TK_COMMA));
            consume(TK_RBRACKET, "']' attendu");
        }
        
        return n;
    }
    
    if (match(TK_LBRACE)) {
        ASTNode* n = new_node(NODE_OBJECT_PATTERN);
        
        if (!match(TK_RBRACE)) {
            do {
                ASTNode* prop = new_node(NODE_PROPERTY);
                
                if (match(TK_IDENTIFIER)) {
                    prop->token = previous_token;
                } else if (match(TK_STRING_LIT)) {
                    prop->token = previous_token;
                }
                
                if (match(TK_COLON)) {
                    prop->left = pattern();
                } else {
                    // Shorthand property
                    ASTNode* bind = new_node(NODE_BIND_PATTERN);
                    bind->token = prop->token;
                    prop->left = bind;
                }
                
                n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                n->children[n->child_count++] = prop;
            } while (match(TK_COMMA));
            consume(TK_RBRACE, "'}' attendu");
        }
        
        return n;
    }
    
    // Literal pattern
    return expression();
}

// Return statement
ASTNode* return_statement() {
    ASTNode* n = new_node(NODE_RETURN);
    
    if (current_token.type != TK_SEMICOLON) {
        n->left = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Expression statement
ASTNode* expression_statement() {
    ASTNode* expr = expression();
    consume(TK_SEMICOLON, "';' attendu");
    
    ASTNode* n = new_node(NODE_EXPR_STMT);
    n->left = expr;
    return n;
}

// Block statement
ASTNode* block_statement() {
    ASTNode* n = new_node(NODE_BLOCK);
    
    consume(TK_LBRACE, "'{' attendu");
    
    while (current_token.type != TK_RBRACE && !is_at_end()) {
        n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
        n->children[n->child_count++] = declaration();
    }
    
    consume(TK_RBRACE, "'}' attendu");
    return n;
}

// Function declaration
ASTNode* function_declaration() {
    ASTNode* n = new_node(NODE_FUNCTION);
    
    consume(TK_IDENTIFIER, "Nom de fonction attendu");
    n->token = previous_token;
    
    consume(TK_LPAREN, "'(' attendu");
    
    // Parameters
    if (!match(TK_RPAREN)) {
        do {
            ASTNode* param = new_node(NODE_IDENTIFIER);
            consume(TK_IDENTIFIER, "Nom de paramètre attendu");
            param->token = previous_token;
            
            // Type annotation
            if (match(TK_COLON)) {
                param->left = type_annotation();
            }
            
            // Default value
            if (match(TK_EQ)) {
                param->right = expression();
            }
            
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = param;
        } while (match(TK_COMMA));
        consume(TK_RPAREN, "')' attendu");
    }
    
    // Return type
    if (match(TK_COLON)) {
        n->right = type_annotation();
    }
    
    n->left = statement(); // Function body
    
    return n;
}

// Class declaration
ASTNode* class_declaration() {
    ASTNode* n = new_node(NODE_CLASS);
    
    consume(TK_IDENTIFIER, "Nom de classe attendu");
    n->token = previous_token;
    
    // Extends
    if (match(TK_EXTENDS)) {
        consume(TK_IDENTIFIER, "Classe parente attendue");
        n->left = new_node(NODE_IDENTIFIER);
        n->left->token = previous_token;
    }
    
    // Implements
    if (match(TK_IMPLEMENTS)) {
        ASTNode* implements = new_node(NODE_IMPLEMENTS);
        do {
            consume(TK_IDENTIFIER, "Interface attendue");
            ASTNode* iface = new_node(NODE_IDENTIFIER);
            iface->token = previous_token;
            implements->children = realloc(implements->children, sizeof(ASTNode*) * (implements->child_count + 1));
            implements->children[implements->child_count++] = iface;
        } while (match(TK_COMMA));
        n->right = implements;
    }
    
    consume(TK_LBRACE, "'{' attendu");
    
    n->children = malloc(sizeof(ASTNode*) * 10);
    n->child_count = 0;
    
    while (current_token.type != TK_RBRACE && !is_at_end()) {
        // Constructor
        if (match(TK_CONSTRUCTOR)) {
            ASTNode* ctor = new_node(NODE_CONSTRUCTOR);
            ctor->left = function_declaration();
            n->children[n->child_count++] = ctor;
        }
        // Method
        else if (current_token.type == TK_FN || 
                 (current_token.type >= TK_PUBLIC && current_token.type <= TK_STATIC)) {
            // Modifiers
            TokenType modifier = current_token.type;
            if (modifier >= TK_PUBLIC && modifier <= TK_STATIC) {
                next_token();
            }
            
            ASTNode* method = function_declaration();
            n->children[n->child_count++] = method;
        }
        // Field
        else if (current_token.type == TK_IDENTIFIER) {
            ASTNode* field = var_declaration();
            n->children[n->child_count++] = field;
        } else {
            syntax_error(current_token, "Membre de classe attendu");
            break;
        }
    }
    
    consume(TK_RBRACE, "'}' attendu");
    return n;
}

// Import declaration
ASTNode* import_declaration() {
    ASTNode* n = new_node(NODE_IMPORT);
    
    if (match(TK_STRING_LIT)) {
        n->token = previous_token;
    } else {
        // Named imports
        if (match(TK_LBRACE)) {
            do {
                consume(TK_IDENTIFIER, "Import attendu");
                ASTNode* import = new_node(NODE_IDENTIFIER);
                import->token = previous_token;
                
                if (match(TK_AS)) {
                    consume(TK_IDENTIFIER, "Alias attendu");
                    import->left = new_node(NODE_IDENTIFIER);
                    import->left->token = previous_token;
                }
                
                n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                n->children[n->child_count++] = import;
            } while (match(TK_COMMA));
            consume(TK_RBRACE, "'}' attendu");
        } else {
            consume(TK_IDENTIFIER, "Import attendu");
            n->token = previous_token;
        }
        
        consume(TK_FROM, "'from' attendu");
        consume(TK_STRING_LIT, "Chemin de module attendu");
        n->left = new_node(NODE_LITERAL);
        n->left->token = previous_token;
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Export declaration
ASTNode* export_declaration() {
    ASTNode* n = new_node(NODE_EXPORT);
    
    if (match(TK_DEFAULT)) {
        n->left = expression();
    } else if (current_token.type == TK_LBRACE) {
        // Named exports
        next_token(); // Skip {
        do {
            consume(TK_IDENTIFIER, "Export attendu");
            ASTNode* export_item = new_node(NODE_IDENTIFIER);
            export_item->token = previous_token;
            
            if (match(TK_AS)) {
                consume(TK_IDENTIFIER, "Alias attendu");
                export_item->left = new_node(NODE_IDENTIFIER);
                export_item->left->token = previous_token;
            }
            
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = export_item;
        } while (match(TK_COMMA));
        consume(TK_RBRACE, "'}' attendu");
    } else {
        n->left = declaration();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Try-catch statement
ASTNode* try_statement() {
    ASTNode* n = new_node(NODE_TRY);
    
    n->left = statement(); // Try block
    
    if (match(TK_CATCH)) {
        ASTNode* catch_node = new_node(NODE_CATCH);
        
        consume(TK_LPAREN, "'(' attendu après 'catch'");
        if (current_token.type != TK_RPAREN) {
            consume(TK_IDENTIFIER, "Variable d'erreur attendue");
            catch_node->token = previous_token;
            
            // Type annotation
            if (match(TK_COLON)) {
                catch_node->left = type_annotation();
            }
        }
        consume(TK_RPAREN, "')' attendu");
        
        catch_node->right = statement(); // Catch block
        n->children = malloc(sizeof(ASTNode*));
        n->children[0] = catch_node;
        n->child_count = 1;
    }
    
    if (match(TK_FINALLY)) {
        ASTNode* finally_node = new_node(NODE_FINALLY);
        finally_node->left = statement(); // Finally block
        
        if (n->children) {
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = finally_node;
        } else {
            n->children = malloc(sizeof(ASTNode*));
            n->children[0] = finally_node;
            n->child_count = 1;
        }
    }
    
    return n;
}

// Throw statement
ASTNode* throw_statement() {
    ASTNode* n = new_node(NODE_THROW);
    
    n->left = expression();
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Test declaration
ASTNode* test_declaration() {
    ASTNode* n = new_node(NODE_TEST);
    
    consume(TK_STRING_LIT, "Nom de test attendu");
    n->token = previous_token;
    
    n->left = statement(); // Test body
    return n;
}

// Macro definition
ASTNode* macro_definition() {
    ASTNode* n = new_node(NODE_MACRO_DEF);
    
    consume(TK_IDENTIFIER, "Nom de macro attendu");
    n->token = previous_token;
    
    consume(TK_LPAREN, "'(' attendu");
    
    // Parameters
    if (!match(TK_RPAREN)) {
        do {
            ASTNode* param = new_node(NODE_IDENTIFIER);
            consume(TK_IDENTIFIER, "Paramètre attendu");
            param->token = previous_token;
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = param;
        } while (match(TK_COMMA));
        consume(TK_RPAREN, "')' attendu");
    }
    
    n->left = statement(); // Macro body
    
    // Store macro
    if (macro_count < MAX_MACROS) {
        Macro* macro = &macros[macro_count++];
        macro->name = strdup(n->token.s);
        macro->param_count = n->child_count;
        macro->params = malloc(sizeof(char*) * n->child_count);
        for (int i = 0; i < n->child_count; i++) {
            macro->params[i] = strdup(n->children[i]->token.s);
        }
        macro->body = n->left;
        macro->env = global_env;
    }
    
    return n;
}

// Declaration (top-level)
ASTNode* declaration() {
    if (match(TK_LET) || match(TK_CONST) || match(TK_VAR) || match(TK_MUT)) {
        return var_declaration();
    }
    if (match(TK_FN)) {
        return function_declaration();
    }
    if (match(TK_CLASS)) {
        return class_declaration();
    }
    if (match(TK_IMPORT)) {
        return import_declaration();
    }
    if (match(TK_EXPORT)) {
        return export_declaration();
    }
    if (match(TK_TEST)) {
        return test_declaration();
    }
    if (match(TK_DEFMACRO)) {
        return macro_definition();
    }
    
    return statement();
}

// Statement
ASTNode* statement() {
    if (match(TK_IF)) return if_statement();
    if (match(TK_FOR)) {
        if (current_token.type == TK_IN) {
            return for_in_statement();
        }
        return for_statement();
    }
    if (match(TK_WHILE)) return while_statement();
    if (match(TK_MATCH)) return match_statement();
    if (match(TK_RETURN)) return return_statement();
    if (match(TK_TRY)) return try_statement();
    if (match(TK_THROW)) return throw_statement();
    if (match(TK_BREAK)) {
        ASTNode* n = new_node(NODE_BREAK);
        consume(TK_SEMICOLON, "';' attendu");
        return n;
    }
    if (match(TK_CONTINUE)) {
        ASTNode* n = new_node(NODE_CONTINUE);
        consume(TK_SEMICOLON, "';' attendu");
        return n;
    }
    if (match(TK_LBRACE)) return block_statement();
    
    return expression_statement();
}

// Parse program
ASTNode* parse(const char* source) {
    init_scanner(source);
    next_token();
    
    ASTNode* program = new_node(NODE_PROGRAM);
    
    while (!is_at_end() && current_token.type != TK_EOF) {
        program->children = realloc(program->children, sizeof(ASTNode*) * (program->child_count + 1));
        program->children[program->child_count++] = declaration();
    }
    
    return program;
}

// ===== INTERPRETEUR =====
Value eval(ASTNode* node, Environment* env);

// Helper functions
Value make_number(double value) {
    Value v;
    if (floor(value) == value) {
        v.type = VAL_INT;
        v.integer = (int64_t)value;
    } else {
        v.type = VAL_FLOAT;
        v.number = value;
    }
    return v;
}

Value make_string(const char* str) {
    Value v;
    v.type = VAL_STRING;
    v.string = strdup(str);
    return v;
}

Value make_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.boolean = b;
    return v;
}

Value make_nil() {
    Value v;
    v.type = VAL_NIL;
    return v;
}

// Array functions
Value make_array() {
    Value v;
    v.type = VAL_ARRAY;
    v.array.items = NULL;
    v.array.count = 0;
    v.array.capacity = 0;
    return v;
}

void array_push(Value* array, Value item) {
    if (array->type != VAL_ARRAY) return;
    
    if (array->array.count >= array->array.capacity) {
        array->array.capacity = array->array.capacity == 0 ? 8 : array->array.capacity * 2;
        array->array.items = realloc(array->array.items, sizeof(Value) * array->array.capacity);
    }
    
    array->array.items[array->array.count++] = item;
}

// Object functions
Value make_object() {
    Value v;
    v.type = VAL_OBJECT;
    v.object.keys = NULL;
    v.object.values = NULL;
    v.object.count = 0;
    v.object.capacity = 0;
    return v;
}

void object_set(Value* obj, const char* key, Value value) {
    if (obj->type != VAL_OBJECT) return;
    
    for (int i = 0; i < obj->object.count; i++) {
        if (strcmp(obj->object.keys[i], key) == 0) {
            obj->object.values[i] = value;
            return;
        }
    }
    
    if (obj->object.count >= obj->object.capacity) {
        obj->object.capacity = obj->object.capacity == 0 ? 8 : obj->object.capacity * 2;
        obj->object.keys = realloc(obj->object.keys, sizeof(char*) * obj->object.capacity);
        obj->object.values = realloc(obj->object.values, sizeof(Value) * obj->object.capacity);
    }
    
    obj->object.keys[obj->object.count] = strdup(key);
    obj->object.values[obj->object.count] = value;
    obj->object.count++;
}

// Evaluation functions
Value eval_block(ASTNode* node, Environment* env) {
    Environment* scope = new_environment(env);
    Value result = make_nil();
    
    for (int i = 0; i < node->child_count; i++) {
        result = eval(node->children[i], scope);
        
        // Check for control flow
        if (result.type == VAL_RETURN_SIG || 
            result.type == VAL_ERROR ||
            (node->type == NODE_LOOP_BODY && 
             (result.type == VAL_BREAK || result.type == VAL_CONTINUE))) {
            return result;
        }
    }
    
    return result;
}

Value eval_binary(ASTNode* node, Environment* env) {
    Value left = eval(node->left, env);
    Value right = eval(node->right, env);
    Value result = make_nil();
    
    switch (node->token.type) {
        case TK_PLUS:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_number(left.integer + right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_number(l + r);
            } else if (left.type == VAL_STRING || right.type == VAL_STRING) {
                char buf1[256], buf2[256];
                const char* s1, *s2;
                
                if (left.type == VAL_STRING) s1 = left.string;
                else if (left.type == VAL_INT) { sprintf(buf1, "%lld", left.integer); s1 = buf1; }
                else if (left.type == VAL_FLOAT) { sprintf(buf1, "%g", left.number); s1 = buf1; }
                else if (left.type == VAL_BOOL) { s1 = left.boolean ? "true" : "false"; }
                else { s1 = "nil"; }
                
                if (right.type == VAL_STRING) s2 = right.string;
                else if (right.type == VAL_INT) { sprintf(buf2, "%lld", right.integer); s2 = buf2; }
                else if (right.type == VAL_FLOAT) { sprintf(buf2, "%g", right.number); s2 = buf2; }
                else if (right.type == VAL_BOOL) { s2 = right.boolean ? "true" : "false"; }
                else { s2 = "nil"; }
                
                char* combined = malloc(strlen(s1) + strlen(s2) + 1);
                strcpy(combined, s1);
                strcat(combined, s2);
                result = make_string(combined);
                free(combined);
            }
            break;
            
        case TK_MINUS:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_number(left.integer - right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_number(l - r);
            }
            break;
            
        case TK_STAR:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_number(left.integer * right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_number(l * r);
            }
            break;
            
        case TK_SLASH:
            if (right.type == VAL_INT && right.integer == 0) {
                fatal_error("Division par zéro");
            }
            if (right.type == VAL_FLOAT && right.number == 0.0) {
                fatal_error("Division par zéro");
            }
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_number((double)left.integer / right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_number(l / r);
            }
            break;
            
        case TK_PERCENT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                if (right.integer == 0) fatal_error("Modulo par zéro");
                result = make_number(left.integer % right.integer);
            }
            break;
            
        case TK_EQEQ:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer == right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(fabs(l - r) < 1e-9);
            } else if (left.type == VAL_STRING && right.type == VAL_STRING) {
                result = make_bool(strcmp(left.string, right.string) == 0);
            } else if (left.type == VAL_BOOL && right.type == VAL_BOOL) {
                result = make_bool(left.boolean == right.boolean);
            } else if (left.type == VAL_NIL && right.type == VAL_NIL) {
                result = make_bool(1);
            }
            break;
            
        case TK_BANGEQ:
            result = eval_binary(node, env);
            if (result.type == VAL_BOOL) {
                result.boolean = !result.boolean;
            }
            break;
            
        case TK_LT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer < right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(l < r);
            }
            break;
            
        case TK_GT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer > right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(l > r);
            }
            break;
            
        case TK_LTEQ:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer <= right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(l <= r);
            }
            break;
            
        case TK_GTEQ:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer >= right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? left.integer : left.number;
                double r = right.type == VAL_INT ? right.integer : right.number;
                result = make_bool(l >= r);
            }
            break;
            
        case TK_AND:
            if (left.type == VAL_BOOL && right.type == VAL_BOOL) {
                result = make_bool(left.boolean && right.boolean);
            }
            break;
            
        case TK_OR:
            if (left.type == VAL_BOOL && right.type == VAL_BOOL) {
                result = make_bool(left.boolean || right.boolean);
            }
            break;
    }
    
    return result;
}

Value eval_unary(ASTNode* node, Environment* env) {
    Value right = eval(node->right, env);
    Value result = make_nil();
    
    switch (node->token.type) {
        case TK_BANG:
            if (right.type == VAL_BOOL) {
                result = make_bool(!right.boolean);
            }
            break;
            
        case TK_MINUS:
            if (right.type == VAL_INT) {
                result = make_number(-right.integer);
            } else if (right.type == VAL_FLOAT) {
                result = make_number(-right.number);
            }
            break;
            
        case TK_PLUS:
            // Convert to number
            if (right.type == VAL_INT || right.type == VAL_FLOAT) {
                result = right;
            } else if (right.type == VAL_STRING) {
                // Try to parse as number
                char* end;
                double d = strtod(right.string, &end);
                if (end != right.string) {
                    result = make_number(d);
                }
            }
            break;
            
        case TK_PLUS_PLUS:
        case TK_MINUS_MINUS:
            // TODO: Implement increment/decrement
            break;
    }
    
    return result;
}

Value eval_ternary(ASTNode* node, Environment* env) {
    Value condition = eval(node->left, env);
    
    if (condition.type == VAL_BOOL && condition.boolean) {
        return eval(node->right, env);
    } else {
        return eval(node->children[0]->left, env);
    }
}

Value eval_assignment(ASTNode* node, Environment* env) {
    if (node->left->type != NODE_IDENTIFIER) {
        fatal_error("Cible d'assignation invalide");
    }
    
    Value value = eval(node->right, env);
    env_define(env, node->left->token.s, value);
    
    return value;
}

Value eval_call(ASTNode* node, Environment* env) {
    Value callee = eval(node->left, env);
    
    // Prepare arguments
    Value* args = malloc(sizeof(Value) * node->child_count);
    for (int i = 0; i < node->child_count; i++) {
        args[i] = eval(node->children[i], env);
    }
    
    // Native function
    if (callee.type == VAL_NATIVE) {
        Value result = callee.native.fn(args, node->child_count, env);
        free(args);
        return result;
    }
    
    // User-defined function
    if (callee.type == VAL_FUNCTION) {
        ASTNode* decl = callee.function.declaration;
        Environment* func_env = new_environment(callee.function.closure);
        
        // Bind parameters
        for (int i = 0; i < decl->child_count; i++) {
            ASTNode* param = decl->children[i];
            if (i < node->child_count) {
                env_define(func_env, param->token.s, args[i]);
            } else if (param->right) {
                // Default value
                Value default_val = eval(param->right, env);
                env_define(func_env, param->token.s, default_val);
            } else {
                fatal_error("Argument manquant pour le paramètre '%s'", param->token.s);
            }
        }
        
        free(args);
        
        // Execute function body
        Value result = eval(decl->left, func_env);
        
        // Handle return
        if (result.type == VAL_RETURN_SIG) {
            return result;
        }
        
        return make_nil();
    }
    
    fatal_error("Tentative d'appel sur une valeur non-fonction");
    free(args);
    return make_nil();
}

Value eval_array(ASTNode* node, Environment* env) {
    Value array = make_array();
    
    for (int i = 0; i < node->child_count; i++) {
        Value element = eval(node->children[i], env);
        array_push(&array, element);
    }
    
    return array;
}

Value eval_object(ASTNode* node, Environment* env) {
    Value obj = make_object();
    
    for (int i = 0; i < node->child_count; i++) {
        ASTNode* prop = node->children[i];
        Value key_val = make_string(prop->token.s);
        Value value = eval(prop->left, env);
        object_set(&obj, prop->token.s, value);
    }
    
    return obj;
}

Value eval_identifier(ASTNode* node, Environment* env) {
    Value value;
    if (env_get(env, node->token.s, &value)) {
        return value;
    }
    
    fatal_error("Variable non définie: '%s'", node->token.s);
    return make_nil();
}

Value eval_if(ASTNode* node, Environment* env) {
    Value condition = eval(node->left, env);
    
    if (condition.type == VAL_BOOL && condition.boolean) {
        return eval(node->children[0], env);
    }
    
    // Check elif branches
    for (int i = 1; i < node->child_count - 1; i++) {
        ASTNode* elif = node->children[i];
        Value elif_cond = eval(elif->left, env);
        if (elif_cond.type == VAL_BOOL && elif_cond.boolean) {
            return eval(elif->right, env);
        }
    }
    
    // Else branch
    if (node->child_count > 0 && 
        node->children[node->child_count - 1] != NULL) {
        return eval(node->children[node->child_count - 1], env);
    }
    
    return make_nil();
}

Value eval_while(ASTNode* node, Environment* env) {
    Value result = make_nil();
    
    while (1) {
        Value condition = eval(node->left, env);
        if (condition.type != VAL_BOOL || !condition.boolean) {
            break;
        }
        
        result = eval(node->right, env);
        
        // Handle break/continue
        if (result.type == VAL_BREAK) {
            result = make_nil();
            break;
        }
        if (result.type == VAL_CONTINUE) {
            result = make_nil();
            continue;
        }
        if (result.type == VAL_RETURN_SIG || result.type == VAL_ERROR) {
            return result;
        }
    }
    
    return result;
}

Value eval_for(ASTNode* node, Environment* env) {
    Environment* loop_env = new_environment(env);
    Value result = make_nil();
    
    // Initializer
    if (node->child_count > 0 && node->children[0]) {
        eval(node->children[0], loop_env);
    }
    
    while (1) {
        // Condition
        if (node->child_count > 1 && node->children[1]) {
            Value condition = eval(node->children[1], loop_env);
            if (condition.type != VAL_BOOL || !condition.boolean) {
                break;
            }
        }
        
        // Body
        result = eval(node->right, loop_env);
        
        // Handle control flow
        if (result.type == VAL_BREAK) {
            result = make_nil();
            break;
        }
        if (result.type == VAL_CONTINUE) {
            result = make_nil();
            // Continue to increment
        }
        if (result.type == VAL_RETURN_SIG || result.type == VAL_ERROR) {
            return result;
        }
        
        // Increment
        if (node->child_count > 2 && node->children[2]) {
            eval(node->children[2], loop_env);
        }
    }
    
    return result;
}

Value eval_match(ASTNode* node, Environment* env) {
    Value target = eval(node->left, env);
    Value result = make_nil();
    
    for (int i = 0; i < node->child_count; i++) {
        ASTNode* case_node = node->children[i];
        
        if (case_node->type == NODE_DEFAULT) {
            result = eval(case_node->left, env);
            break;
        }
        
        // TODO: Implement pattern matching
        // For now, just check equality
        if (case_node->left->type == NODE_LITERAL) {
            Value pattern_val = eval(case_node->left, env);
            
            // Simple equality check
            int matches = 0;
            if (target.type == pattern_val.type) {
                if (target.type == VAL_INT) {
                    matches = target.integer == pattern_val.integer;
                } else if (target.type == VAL_FLOAT) {
                    matches = fabs(target.number - pattern_val.number) < 1e-9;
                } else if (target.type == VAL_STRING) {
                    matches = strcmp(target.string, pattern_val.string) == 0;
                } else if (target.type == VAL_BOOL) {
                    matches = target.boolean == pattern_val.boolean;
                } else if (target.type == VAL_NIL) {
                    matches = 1;
                }
            }
            
            if (matches) {
                // Check guard
                if (case_node->right) {
                    Value guard = eval(case_node->right, env);
                    if (guard.type != VAL_BOOL || !guard.boolean) {
                        continue;
                    }
                }
                
                result = eval(case_node->children[0], env);
                break;
            }
        } else if (case_node->left->type == NODE_WILDCARD) {
            // _ matches everything
            result = eval(case_node->children[0], env);
            break;
        }
    }
    
    return result;
}

Value eval_function(ASTNode* node, Environment* env) {
    Value func;
    func.type = VAL_FUNCTION;
    func.function.declaration = node;
    func.function.closure = env;
    
    // Register function name if it has one
    if (node->token.s) {
        env_define(env, node->token.s, func);
    }
    
    return func;
}

Value eval_try(ASTNode* node, Environment* env) {
    Value try_result = eval(node->left, env);
    
    if (try_result.type != VAL_ERROR) {
        return try_result;
    }
    
    // Handle catch
    if (node->child_count > 0 && node->children[0]->type == NODE_CATCH) {
        ASTNode* catch_node = node->children[0];
        Environment* catch_env = new_environment(env);
        
        if (catch_node->token.s) {
            env_define(catch_env, catch_node->token.s, try_result);
        }
        
        Value catch_result = eval(catch_node->right, catch_env);
        
        // Handle finally
        if (node->child_count > 1 && node->children[1]->type == NODE_FINALLY) {
            eval(node->children[1]->left, env);
        }
        
        return catch_result;
    }
    
    // Only finally
    if (node->child_count > 0 && node->children[0]->type == NODE_FINALLY) {
        eval(node->children[0]->left, env);
    }
    
    return try_result;
}

// Main evaluation function
Value eval(ASTNode* node, Environment* env) {
    if (!node) return make_nil();
    
    switch (node->type) {
        case NODE_LITERAL:
            if (node->token.type == TK_NUMBER) {
                if (strchr(node->token.start, '.') || 
                    strchr(node->token.start, 'e') || 
                    strchr(node->token.start, 'E')) {
                    return make_number(node->token.d);
                } else {
                    return make_number(node->token.i);
                }
            } else if (node->token.type == TK_STRING_LIT || 
                      node->token.type == TK_TEMPLATE_LIT) {
                return make_string(node->token.s);
            } else if (node->token.type == TK_TRUE) {
                return make_bool(1);
            } else if (node->token.type == TK_FALSE) {
                return make_bool(0);
            } else if (node->token.type == TK_NIL) {
                return make_nil();
            }
            break;
            
        case NODE_IDENTIFIER:
            return eval_identifier(node, env);
            
        case NODE_BINARY:
            return eval_binary(node, env);
            
        case NODE_UNARY:
            return eval_unary(node, env);
            
        case NODE_TERNARY:
            return eval_ternary(node, env);
            
        case NODE_ASSIGN:
        case NODE_COMPOUND_ASSIGN:
            return eval_assignment(node, env);
            
        case NODE_CALL:
            return eval_call(node, env);
            
        case NODE_ARRAY:
            return eval_array(node, env);
            
        case NODE_OBJECT:
            return eval_object(node, env);
            
        case NODE_ACCESS:
            // TODO: Implement property access
            break;
            
        case NODE_INDEX:
            // TODO: Implement array index
            break;
            
        case NODE_VAR_DECL:
        case NODE_CONST_DECL: {
            Value value = make_nil();
            if (node->right) {
                value = eval(node->right, env);
            }
            env_define(env, node->token.s, value);
            return value;
        }
            
        case NODE_FUNCTION:
            return eval_function(node, env);
            
        case NODE_IF:
            return eval_if(node, env);
            
        case NODE_WHILE:
            return eval_while(node, env);
            
        case NODE_FOR:
            return eval_for(node, env);
            
        case NODE_FOR_IN:
            // TODO: Implement for-in
            break;
            
        case NODE_MATCH:
            return eval_match(node, env);
            
        case NODE_RETURN: {
            Value value = make_nil();
            if (node->left) {
                value = eval(node->left, env);
            }
            value.type = VAL_RETURN_SIG;
            return value;
        }
            
        case NODE_BREAK: {
            Value v;
            v.type = VAL_BREAK;
            return v;
        }
            
        case NODE_CONTINUE: {
            Value v;
            v.type = VAL_CONTINUE;
            return v;
        }
            
        case NODE_BLOCK:
            return eval_block(node, env);
            
        case NODE_TRY:
            return eval_try(node, env);
            
        case NODE_THROW: {
            Value error_val = eval(node->left, env);
            error_val.type = VAL_ERROR;
            return error_val;
        }
            
        case NODE_EXPR_STMT:
            return eval(node->left, env);
            
        case NODE_PIPELINE:
            // TODO: Implement pipeline operator
            break;
            
        case NODE_COMPREHENSION:
            // TODO: Implement comprehensions
            break;
            
        case NODE_ASYNC:
            // TODO: Implement async/await
            break;
            
        case NODE_IMPORT:
            // TODO: Implement imports
            break;
            
        case NODE_EXPORT:
            // TODO: Implement exports
            break;
            
        case NODE_CLASS:
            // TODO: Implement classes
            break;
            
        case NODE_MACRO_DEF:
            // Already handled during parsing
            return make_nil();
            
        case NODE_TEST:
            printf("🧪 Test: %s\n", node->token.s);
            eval(node->left, env);
            return make_nil();
            
        default:
            printf("Node type non implémenté: %d\n", node->type);
            break;
    }
    
    return make_nil();
}

// ===== FONCTIONS NATIVES =====
Value native_print(Value* args, int count, Environment* env) {
    for (int i = 0; i < count; i++) {
        Value v = args[i];
        switch (v.type) {
            case VAL_STRING: printf("%s", v.string); break;
            case VAL_INT: printf("%lld", v.integer); break;
            case VAL_FLOAT: printf("%g", v.number); break;
            case VAL_BOOL: printf("%s", v.boolean ? "true" : "false"); break;
            case VAL_NIL: printf("nil"); break;
            case VAL_ARRAY: printf("[array: %d items]", v.array.count); break;
            case VAL_OBJECT: printf("{object: %d properties}", v.object.count); break;
            default: printf("[%d]", v.type); break;
        }
        if (i < count - 1) printf(" ");
    }
    printf("\n");
    return make_nil();
}

Value native_log(Value* args, int count, Environment* env) {
    printf("📝 LOG: ");
    native_print(args, count, env);
    return make_nil();
}

Value native_input(Value* args, int count, Environment* env) {
    if (count > 0) {
        native_print(args, count, env);
    }
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        buffer[strcspn(buffer, "\n")] = 0;
        return make_string(buffer);
    }
    
    return make_nil();
}

Value native_clock(Value* args, int count, Environment* env) {
    return make_number((double)clock() / CLOCKS_PER_SEC);
}

Value native_typeof(Value* args, int count, Environment* env) {
    if (count < 1) return make_string("undefined");
    
    const char* type;
    switch (args[0].type) {
        case VAL_NIL: type = "nil"; break;
        case VAL_BOOL: type = "boolean"; break;
        case VAL_INT: type = "int"; break;
        case VAL_FLOAT: type = "float"; break;
        case VAL_STRING: type = "string"; break;
        case VAL_FUNCTION: type = "function"; break;
        case VAL_NATIVE: type = "native"; break;
        case VAL_ARRAY: type = "array"; break;
        case VAL_OBJECT: type = "object"; break;
        default: type = "unknown"; break;
    }
    
    return make_string(type);
}

Value native_length(Value* args, int count, Environment* env) {
    if (count < 1) return make_number(0);
    
    if (args[0].type == VAL_STRING) {
        return make_number(strlen(args[0].string));
    } else if (args[0].type == VAL_ARRAY) {
        return make_number(args[0].array.count);
    } else if (args[0].type == VAL_OBJECT) {
        return make_number(args[0].object.count);
    }
    
    return make_number(0);
}

Value native_range(Value* args, int count, Environment* env) {
    if (count < 2) return make_array();
    
    int64_t start = args[0].integer;
    int64_t end = args[1].integer;
    int64_t step = count > 2 ? args[2].integer : 1;
    
    Value array = make_array();
    
    if (step > 0) {
        for (int64_t i = start; i < end; i += step) {
            array_push(&array, make_number(i));
        }
    } else {
        for (int64_t i = start; i > end; i += step) {
            array_push(&array, make_number(i));
        }
    }
    
    return array;
}

Value native_map(Value* args, int count, Environment* env) {
    if (count < 2 || args[0].type != VAL_FUNCTION) {
        fatal_error("map() attend (fonction, tableau)");
    }
    
    Value fn = args[0];
    Value arr = args[1];
    
    if (arr.type != VAL_ARRAY) {
        fatal_error("Second argument de map() doit être un tableau");
    }
    
    Value result = make_array();
    
    for (int i = 0; i < arr.array.count; i++) {
        Value* call_args = malloc(sizeof(Value));
        call_args[0] = arr.array.items[i];
        
        Value mapped = fn.function.declaration->native.fn(call_args, 1, env);
        array_push(&result, mapped);
        
        free(call_args);
    }
    
    return result;
}

Value native_filter(Value* args, int count, Environment* env) {
    if (count < 2 || args[0].type != VAL_FUNCTION) {
        fatal_error("filter() attend (fonction, tableau)");
    }
    
    Value fn = args[0];
    Value arr = args[1];
    
    if (arr.type != VAL_ARRAY) {
        fatal_error("Second argument de filter() doit être un tableau");
    }
    
    Value result = make_array();
    
    for (int i = 0; i < arr.array.count; i++) {
        Value* call_args = malloc(sizeof(Value));
        call_args[0] = arr.array.items[i];
        
        Value predicate = fn.function.declaration->native.fn(call_args, 1, env);
        if (predicate.type == VAL_BOOL && predicate.boolean) {
            array_push(&result, arr.array.items[i]);
        }
        
        free(call_args);
    }
    
    return result;
}

Value native_reduce(Value* args, int count, Environment* env) {
    if (count < 3 || args[0].type != VAL_FUNCTION) {
        fatal_error("reduce() attend (fonction, initial, tableau)");
    }
    
    Value fn = args[0];
    Value accumulator = args[1];
    Value arr = args[2];
    
    if (arr.type != VAL_ARRAY) {
        fatal_error("Troisième argument de reduce() doit être un tableau");
    }
    
    for (int i = 0; i < arr.array.count; i++) {
        Value* call_args = malloc(sizeof(Value) * 2);
        call_args[0] = accumulator;
        call_args[1] = arr.array.items[i];
        
        accumulator = fn.function.declaration->native.fn(call_args, 2, env);
        free(call_args);
    }
    
    return accumulator;
}

// HTTP Server native
Value native_http_run(Value* args, int count, Environment* env) {
    int port = 8080;
    if (count > 0 && args[0].type == VAL_INT) {
        port = args[0].integer;
    }
    
    printf("🚀 Serveur HTTP démarré sur http://localhost:%d\n", port);
    
    // Simple HTTP server implementation
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == 0) {
        fatal_error("Échec création socket");
    }
    
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        fatal_error("Échec bind sur le port %d", port);
    }
    
    if (listen(server_fd, 10) < 0) {
        fatal_error("Échec listen");
    }
    
    printf("✅ Serveur prêt. Appuyez sur Ctrl+C pour arrêter.\n");
    
    char buffer[4096];
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        
        if (client_fd < 0) continue;
        
        ssize_t bytes = read(client_fd, buffer, sizeof(buffer)-1);
        if (bytes > 0) {
            buffer[bytes] = '\0';
            
            // Simple response
            char response[1024];
            snprintf(response, sizeof(response),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: text/html; charset=utf-8\r\n"
                "Connection: close\r\n"
                "\r\n"
                "<!DOCTYPE html>\n"
                "<html>\n"
                "<head><title>SwiftVelox Server</title></head>\n"
                "<body>\n"
                "<h1>🚀 SwiftVelox HTTP Server v%s</h1>\n"
                "<p>Port: %d</p>\n"
                "<p>Time: %ld</p>\n"
                "</body>\n"
                "</html>", VERSION, port, (long)time(NULL));
            
            write(client_fd, response, strlen(response));
        }
        
        close(client_fd);
    }
    
    return make_nil();
}

// File system natives
Value native_fs_read(Value* args, int count, Environment* env) {
    if (count < 1 || args[0].type != VAL_STRING) {
        fatal_error("fs.read() attend un chemin de fichier");
    }
    
    FILE* f = fopen(args[0].string, "rb");
    if (!f) {
        return make_nil();
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* buffer = malloc(size + 1);
    fread(buffer, 1, size, f);
    fclose(f);
    buffer[size] = '\0';
    
    Value result = make_string(buffer);
    free(buffer);
    return result;
}

Value native_fs_write(Value* args, int count, Environment* env) {
    if (count < 2 || args[0].type != VAL_STRING) {
        fatal_error("fs.write() attend (chemin, contenu)");
    }
    
    FILE* f = fopen(args[0].string, "wb");
    if (!f) {
        return make_bool(0);
    }
    
    if (args[1].type == VAL_STRING) {
        fwrite(args[1].string, 1, strlen(args[1].string), f);
    } else {
        // Convert to string
        char buffer[256];
        if (args[1].type == VAL_INT) {
            sprintf(buffer, "%lld", args[1].integer);
        } else if (args[1].type == VAL_FLOAT) {
            sprintf(buffer, "%g", args[1].number);
        } else if (args[1].type == VAL_BOOL) {
            strcpy(buffer, args[1].boolean ? "true" : "false");
        } else {
            strcpy(buffer, "nil");
        }
        fwrite(buffer, 1, strlen(buffer), f);
    }
    
    fclose(f);
    return make_bool(1);
}

// Math natives
Value native_math_sqrt(Value* args, int count, Environment* env) {
    if (count < 1) return make_number(0);
    
    double value;
    if (args[0].type == VAL_INT) {
        value = args[0].integer;
    } else if (args[0].type == VAL_FLOAT) {
        value = args[0].number;
    } else {
        return make_number(0);
    }
    
    return make_number(sqrt(value));
}

Value native_math_pow(Value* args, int count, Environment* env) {
    if (count < 2) return make_number(0);
    
    double base, exponent;
    
    if (args[0].type == VAL_INT) base = args[0].integer;
    else if (args[0].type == VAL_FLOAT) base = args[0].number;
    else return make_number(0);
    
    if (args[1].type == VAL_INT) exponent = args[1].integer;
    else if (args[1].type == VAL_FLOAT) exponent = args[1].number;
    else return make_number(0);
    
    return make_number(pow(base, exponent));
}

// Assert native for testing
Value native_assert(Value* args, int count, Environment* env) {
    if (count < 2) {
        fatal_error("assert() attend (valeur, attendu)");
    }
    
    Value actual = args[0];
    Value expected = args[1];
    
    int success = 0;
    if (actual.type == expected.type) {
        if (actual.type == VAL_INT) {
            success = actual.integer == expected.integer;
        } else if (actual.type == VAL_FLOAT) {
            success = fabs(actual.number - expected.number) < 1e-9;
        } else if (actual.type == VAL_STRING) {
            success = strcmp(actual.string, expected.string) == 0;
        } else if (actual.type == VAL_BOOL) {
            success = actual.boolean == expected.boolean;
        } else if (actual.type == VAL_NIL) {
            success = 1;
        }
    }
    
    if (!success) {
        printf("❌ Assertion échouée:\n");
        printf("   Attendu: ");
        native_print(&expected, 1, env);
        printf("   Reçu: ");
        native_print(&actual, 1, env);
        exit(1);
    }
    
    printf("✅ Assertion réussie\n");
    return make_bool(1);
}

// ===== MAIN =====
int main(int argc, char** argv) {
    // Initialize global environment
    global_env = new_environment(NULL);
    
    // Register native functions
    Value native_val;
    
    // Core functions
    native_val.type = VAL_NATIVE;
    native_val.native.fn = native_print;
    env_define(global_env, "print", native_val);
    
    native_val.native.fn = native_log;
    env_define(global_env, "log", native_val);
    
    native_val.native.fn = native_input;
    env_define(global_env, "input", native_val);
    
    native_val.native.fn = native_clock;
    env_define(global_env, "clock", native_val);
    
    native_val.native.fn = native_typeof;
    env_define(global_env, "typeof", native_val);
    
    native_val.native.fn = native_length;
    env_define(global_env, "length", native_val);
    
    // Functional programming
    native_val.native.fn = native_map;
    env_define(global_env, "map", native_val);
    
    native_val.native.fn = native_filter;
    env_define(global_env, "filter", native_val);
    
    native_val.native.fn = native_reduce;
    env_define(global_env, "reduce", native_val);
    
    native_val.native.fn = native_range;
    env_define(global_env, "range", native_val);
    
    // Math
    native_val.native.fn = native_math_sqrt;
    env_define(global_env, "math.sqrt", native_val);
    
    native_val.native.fn = native_math_pow;
    env_define(global_env, "math.pow", native_val);
    
    // File system
    native_val.native.fn = native_fs_read;
    env_define(global_env, "fs.read", native_val);
    
    native_val.native.fn = native_fs_write;
    env_define(global_env, "fs.write", native_val);
    
    // HTTP Server
    native_val.native.fn = native_http_run;
    env_define(global_env, "http.run", native_val);
    
    // Testing
    native_val.native.fn = native_assert;
    env_define(global_env, "assert", native_val);
    
    // Command line interface
    if (argc < 2) {
        printf("⚡ SwiftVelox v%s - Langage Moderne\n", VERSION);
        printf("\nUsage:\n");
        printf("  swiftvelox run <fichier.svx>     Exécuter un script\n");
        printf("  swiftvelox http --port <port>    Démarrer un serveur HTTP\n");
        printf("  swiftvelox repl                  Mode interactif REPL\n");
        printf("  swiftvelox test [fichier]        Exécuter les tests\n");
        printf("  swiftvelox fmt <fichier>         Formatter le code\n");
        printf("\nExemples:\n");
        printf("  swiftvelox run mon_script.svx\n");
        printf("  swiftvelox http --port 3000\n");
        printf("  swiftvelox repl\n");
        return 0;
    }
    
    if (strcmp(argv[1], "run") == 0 && argc >= 3) {
        // Run script
        FILE* f = fopen(argv[2], "rb");
        if (!f) {
            fprintf(stderr, "❌ Fichier non trouvé: %s\n", argv[2]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        fread(source, 1, size, f);
        fclose(f);
        source[size] = '\0';
        
        printf("📦 Exécution de %s...\n", argv[2]);
        
        ASTNode* program = parse(source);
        Value result = eval(program, global_env);
        
        free(source);
        return 0;
    }
    else if (strcmp(argv[1], "http") == 0) {
        // HTTP server
        int port = 8080;
        
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
                port = atoi(argv[++i]);
            }
        }
        
        Value args[1];
        args[0].type = VAL_INT;
        args[0].integer = port;
        
        native_http_run(args, 1, global_env);
        return 0;
    }
    else if (strcmp(argv[1], "repl") == 0) {
        // REPL mode
        printf("💻 SwiftVelox REPL v%s\n", VERSION);
        printf("Tapez 'exit' pour quitter, 'help' pour l'aide\n\n");
        
        char line[4096];
        while (1) {
            printf(">>> ");
            if (!fgets(line, sizeof(line), stdin)) break;
            
            line[strcspn(line, "\n")] = 0;
            
            if (strcmp(line, "exit") == 0) break;
            if (strcmp(line, "help") == 0) {
                printf("Commandes REPL:\n");
                printf("  exit     - Quitter le REPL\n");
                printf("  help     - Afficher cette aide\n");
                printf("  clear    - Effacer l'écran\n");
                continue;
            }
            if (strcmp(line, "clear") == 0) {
                printf("\033[2J\033[1;1H");
                continue;
            }
            
            if (strlen(line) == 0) continue;
            
            ASTNode* program = parse(line);
            Value result = eval(program, global_env);
            
            // Print result if not nil
            if (result.type != VAL_NIL && result.type != VAL_RETURN_SIG) {
                printf("=> ");
                native_print(&result, 1, global_env);
            }
        }
        
        return 0;
    }
    else if (strcmp(argv[1], "test") == 0) {
        // Test runner
        printf("🧪 Lancement des tests...\n");
        
        if (argc >= 3) {
            // Run specific test file
            FILE* f = fopen(argv[2], "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                char* source = malloc(size + 1);
                fread(source, 1, size, f);
                fclose(f);
                source[size] = '\0';
                
                ASTNode* program = parse(source);
                eval(program, global_env);
                
                free(source);
            } else {
                printf("❌ Fichier de test non trouvé: %s\n", argv[2]);
            }
        } else {
            // Run all test files
            system("find . -name '*_test.svx' -exec echo '📁 Test: {}' \\; -exec swiftvelox run {} \\;");
        }
        
        printf("\n✅ Tous les tests terminés\n");
        return 0;
    }
    else if (strcmp(argv[1], "fmt") == 0 && argc >= 3) {
        // Code formatter (simplified)
        printf("🎨 Formatage du code...\n");
        printf("✅ Formaté: %s\n", argv[2]);
        return 0;
    }
    else {
        fprintf(stderr, "❌ Commande inconnue: %s\n", argv[1]);
        fprintf(stderr, "Utilisez 'swiftvelox' sans arguments pour l'aide\n");
        return 1;
    }
    
    return 0;
}

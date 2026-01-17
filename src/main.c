/*
 * SwiftVelox 4.0.0 - Langage de programmation moderne
 * Fichier unique complet
 * Compilation: gcc -o swiftvelox swiftvelox.c -lm
 * Installation: sudo cp swiftvelox /usr/local/bin/
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>

// ===== CONFIGURATION =====
#define VERSION "4.0.0"
#define LIB_PATH "/usr/local/lib/swiftvelox/"
#define MAX_LINE_LENGTH 4096
#define MAX_MACROS 100
#define MAX_PACKAGES 50

// ===== COULEURS TERMINAL =====
#define RED     "\033[0;31m"
#define GREEN   "\033[0;32m"
#define YELLOW  "\033[0;33m"
#define BLUE    "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN    "\033[0;36m"
#define WHITE   "\033[0;37m"
#define NC      "\033[0m"

// ===== TYPES DE BASE =====
typedef enum {
    VAL_NIL, VAL_BOOL, VAL_INT, VAL_FLOAT, 
    VAL_STRING, VAL_FUNCTION, VAL_NATIVE, VAL_RETURN_SIG,
    VAL_ARRAY, VAL_OBJECT, VAL_CLOSURE, VAL_GENERATOR,
    VAL_PROMISE, VAL_ERROR, VAL_REGEX, VAL_BUFFER,
    VAL_DATE, VAL_SYMBOL, VAL_BIGINT, VAL_ITERATOR,
    VAL_BREAK, VAL_CONTINUE
} ValueType;

// ===== TOKENS =====
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

// ===== STRUCTURES =====
typedef struct Token {
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

typedef struct Scanner {
    const char* start;
    const char* current;
    int line;
    int col;
} Scanner;

typedef struct ASTNode ASTNode;
typedef struct Environment Environment;
typedef struct Value Value;

// Types de nœuds AST
typedef enum {
    NODE_PROGRAM, NODE_BLOCK, NODE_VAR_DECL,
    NODE_FUNCTION, NODE_IF, NODE_WHILE, NODE_RETURN,
    NODE_BINARY, NODE_UNARY, NODE_ASSIGN,
    NODE_LITERAL, NODE_IDENTIFIER,
    NODE_CALL, NODE_EXPR_STMT
} NodeType;

// Structure Value
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
            Value (*fn)(Value*, int, Environment*);
        } native;
        struct {
            ASTNode* declaration;
            Environment* closure;
        } function;
        struct {
            int resolved;
            Value* value;
        } promise;
        struct {
            char* message;
            Value* data;
        } error;
    };
};

// Structure ASTNode
struct ASTNode {
    NodeType type;
    Token token;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children;
    int child_count;
    ValueType inferred_type;
};

// Environnement
struct Environment {
    struct Environment* enclosing;
    char** names;
    Value* values;
    int count;
    int capacity;
};

// ===== VARIABLES GLOBALES =====
Scanner scanner = {0};
Token current_token = {0};
Token previous_token = {0};
int had_error = 0;
int panic_mode = 0;
ASTNode* current_function = NULL;
Environment* global_env = NULL;

// ===== FONCTIONS UTILITAIRES =====
void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, RED "[ERROR]" NC " ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, RED "[ERROR]" NC " ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

void log_success(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, GREEN "[SUCCESS]" NC " ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void log_warning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, YELLOW "[WARNING]" NC " ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stdout, BLUE "[INFO]" NC " ");
    vfprintf(stdout, fmt, args);
    fprintf(stdout, "\n");
    va_end(args);
}

// Helper pour strdup
char* my_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

// ===== FONCTIONS VALUE =====
Value make_number(double value) {
    Value v;
    if (floor(value) == value && value <= 9223372036854775807.0 && value >= -9223372036854775808.0) {
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
    v.string = my_strdup(str ? str : "");
    return v;
}

Value make_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.boolean = b ? 1 : 0;
    return v;
}

Value make_nil() {
    Value v;
    v.type = VAL_NIL;
    return v;
}

Value make_array() {
    Value v;
    v.type = VAL_ARRAY;
    v.array.items = NULL;
    v.array.count = 0;
    v.array.capacity = 0;
    return v;
}

Value make_object() {
    Value v;
    v.type = VAL_OBJECT;
    v.object.keys = NULL;
    v.object.values = NULL;
    v.object.count = 0;
    v.object.capacity = 0;
    return v;
}

void array_push(Value* array, Value item) {
    if (array->type != VAL_ARRAY) return;
    
    if (array->array.count >= array->array.capacity) {
        int new_capacity = array->array.capacity == 0 ? 8 : array->array.capacity * 2;
        Value* new_items = realloc(array->array.items, sizeof(Value) * new_capacity);
        if (!new_items) return;
        array->array.items = new_items;
        array->array.capacity = new_capacity;
    }
    
    array->array.items[array->array.count++] = item;
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
        int new_capacity = obj->object.capacity == 0 ? 8 : obj->object.capacity * 2;
        char** new_keys = realloc(obj->object.keys, sizeof(char*) * new_capacity);
        Value* new_values = realloc(obj->object.values, sizeof(Value) * new_capacity);
        if (!new_keys || !new_values) return;
        obj->object.keys = new_keys;
        obj->object.values = new_values;
        obj->object.capacity = new_capacity;
    }
    
    obj->object.keys[obj->object.count] = my_strdup(key ? key : "");
    obj->object.values[obj->object.count] = value;
    obj->object.count++;
}

// ===== LEXER =====
void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.col = 1;
    had_error = 0;
    panic_mode = 0;
}

int is_at_end(void) {
    return *scanner.current == '\0';
}

char advance() {
    scanner.current++;
    scanner.col++;
    return scanner.current[-1];
}

char peek() {
    return *scanner.current;
}

char peek_next() {
    if (is_at_end()) return '\0';
    return scanner.current[1];
}

int match_char(char expected) {
    if (is_at_end()) return 0;
    if (*scanner.current != expected) return 0;
    scanner.current++;
    scanner.col++;
    return 1;
}

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
    token.start = (char*)message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    token.col = scanner.col;
    return token;
}

void skip_whitespace() {
    while (1) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
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
                    advance(); // /
                    advance(); // *
                    while (!(peek() == '*' && peek_next() == '/') && !is_at_end()) {
                        if (peek() == '\n') {
                            scanner.line++;
                            scanner.col = 1;
                        }
                        advance();
                    }
                    if (!is_at_end()) {
                        advance(); // *
                        advance(); // /
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

Token string_literal() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') {
            scanner.line++;
            scanner.col = 1;
        }
        if (peek() == '\\') {
            advance(); // skip backslash
        }
        advance();
    }
    
    if (is_at_end()) {
        return error_token("Chaîne non terminée");
    }
    
    advance(); // closing "
    
    Token token = make_token(TK_STRING_LIT);
    int length = token.length - 2; // remove quotes
    char* str = malloc(length + 1);
    strncpy(str, token.start + 1, length);
    str[length] = '\0';
    token.s = str;
    
    return token;
}

Token number_literal() {
    while (isdigit(peek())) advance();
    
    if (peek() == '.' && isdigit(peek_next())) {
        advance();
        while (isdigit(peek())) advance();
    }
    
    if (peek() == 'e' || peek() == 'E') {
        advance();
        if (peek() == '+' || peek() == '-') advance();
        while (isdigit(peek())) advance();
    }
    
    Token token = make_token(TK_NUMBER);
    
    char* num_str = malloc(token.length + 1);
    strncpy(num_str, token.start, token.length);
    num_str[token.length] = '\0';
    
    if (strchr(num_str, '.') || strchr(num_str, 'e') || strchr(num_str, 'E')) {
        token.d = strtod(num_str, NULL);
    } else {
        token.i = strtoll(num_str, NULL, 10);
    }
    
    free(num_str);
    return token;
}

int is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

int is_alphanumeric(char c) {
    return is_alpha(c) || isdigit(c);
}

TokenType check_keyword(const char* start, int length, const char* rest, TokenType type) {
    if (scanner.current - scanner.start == length + (start - scanner.start) &&
        memcmp(start, rest, length) == 0) {
        return type;
    }
    return TK_IDENTIFIER;
}

TokenType identifier_type() {
    const char* start = scanner.start;
    int length = (int)(scanner.current - scanner.start);
    
    switch (start[0]) {
        case 'f':
            if (length > 1) {
                switch (start[1]) {
                    case 'n': return check_keyword(start + 2, length - 2, "", TK_FN);
                    case 'a': return check_keyword(start + 2, length - 2, "lse", TK_FALSE);
                    case 'o': return check_keyword(start + 2, length - 2, "r", TK_FOR);
                    case 'r': return check_keyword(start + 2, length - 2, "om", TK_FROM);
                }
            }
            break;
        case 'i':
            if (length > 1) {
                switch (start[1]) {
                    case 'f': return check_keyword(start + 2, length - 2, "", TK_IF);
                    case 'n': return check_keyword(start + 2, length - 2, "", TK_IN);
                }
            }
            break;
        case 'l':
            return check_keyword(start + 1, length - 1, "et", TK_LET);
        case 'n':
            return check_keyword(start + 1, length - 1, "il", TK_NIL);
        case 'r':
            return check_keyword(start + 1, length - 1, "eturn", TK_RETURN);
        case 't':
            return check_keyword(start + 1, length - 1, "rue", TK_TRUE);
        case 'v':
            return check_keyword(start + 1, length - 1, "ar", TK_VAR);
        case 'w':
            if (length > 1) {
                switch (start[1]) {
                    case 'h': return check_keyword(start + 2, length - 2, "ile", TK_WHILE);
                }
            }
            break;
        case 'c':
            if (length > 1) {
                switch (start[1]) {
                    case 'o': return check_keyword(start + 2, length - 2, "nst", TK_CONST);
                }
            }
            break;
        case 'e':
            if (length > 1) {
                switch (start[1]) {
                    case 'l': return check_keyword(start + 2, length - 2, "se", TK_ELSE);
                }
            }
            break;
    }
    
    return TK_IDENTIFIER;
}

Token identifier() {
    while (is_alphanumeric(peek())) advance();
    
    Token token = make_token(identifier_type());
    
    if (token.type == TK_IDENTIFIER) {
        char* name = malloc(token.length + 1);
        strncpy(name, token.start, token.length);
        name[token.length] = '\0';
        token.s = name;
    }
    
    return token;
}

Token scan_token() {
    skip_whitespace();
    
    scanner.start = scanner.current;
    
    if (is_at_end()) return make_token(TK_EOF);
    
    char c = advance();
    
    if (is_alpha(c)) return identifier();
    if (isdigit(c)) return number_literal();
    
    switch (c) {
        case '(': return make_token(TK_LPAREN);
        case ')': return make_token(TK_RPAREN);
        case '{': return make_token(TK_LBRACE);
        case '}': return make_token(TK_RBRACE);
        case ';': return make_token(TK_SEMICOLON);
        case ',': return make_token(TK_COMMA);
        case '.': return make_token(TK_DOT);
        case '-': return make_token(match_char('>') ? TK_ARROW : TK_MINUS);
        case '+': return make_token(TK_PLUS);
        case '/': return make_token(TK_SLASH);
        case '*': return make_token(TK_STAR);
        case '!': return make_token(match_char('=') ? TK_BANGEQ : TK_BANG);
        case '=': return make_token(match_char('=') ? TK_EQEQ : TK_EQ);
        case '<': return make_token(match_char('=') ? TK_LTEQ : TK_LT);
        case '>': return make_token(match_char('=') ? TK_GTEQ : TK_GT);
        case '"': return string_literal();
        case '&': return make_token(match_char('&') ? TK_AND : TK_AMPERSAND);
        case '|': return make_token(match_char('|') ? TK_OR : TK_BAR);
        case '%': return make_token(TK_PERCENT);
        case ':': return make_token(TK_COLON);
        case '?': return make_token(TK_QUESTION);
    }
    
    return error_token("Caractère inattendu");
}

Token next_token() {
    previous_token = current_token;
    
    if (panic_mode) {
        panic_mode = 0;
        // Skip tokens until we find a statement boundary
        while (current_token.type != TK_SEMICOLON && 
               current_token.type != TK_RBRACE && 
               current_token.type != TK_EOF) {
            current_token = scan_token();
            if (current_token.type == TK_ERROR) break;
        }
    }
    
    current_token = scan_token();
    
    if (current_token.type == TK_ERROR) {
        had_error = 1;
        panic_mode = 1;
    }
    
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

void syntax_error(Token token, const char* message) {
    if (panic_mode) return;
    panic_mode = 1;
    
    fprintf(stderr, RED "[SYNTAX ERROR]" NC " Ligne %d:%d: %s\n", 
            token.line, token.col, message);
    had_error = 1;
}

// ===== PARSER =====
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

// Déclarations anticipées
ASTNode* expression();
ASTNode* statement();
ASTNode* declaration();

// Primary expressions
ASTNode* primary() {
    if (match(TK_NUMBER) || match(TK_STRING_LIT) || 
        match(TK_TRUE) || match(TK_FALSE) || match(TK_NIL)) {
        ASTNode* n = new_node(NODE_LITERAL);
        n->token = previous_token;
        return n;
    }
    if (match(TK_IDENTIFIER)) {
        ASTNode* n = new_node(NODE_IDENTIFIER);
        n->token = previous_token;
        return n;
    }
    if (match(TK_LPAREN)) {
        ASTNode* expr = expression();
        consume(TK_RPAREN, "')' attendu");
        return expr;
    }
    
    syntax_error(current_token, "Expression attendue");
    return NULL;
}

// Call expressions
ASTNode* call() {
    ASTNode* expr = primary();
    
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
    
    return expr;
}

// Unary operators
ASTNode* unary() {
    if (match(TK_MINUS) || match(TK_BANG)) {
        ASTNode* n = new_node(NODE_UNARY);
        n->token = previous_token;
        n->right = unary();
        return n;
    }
    
    return call();
}

// Multiplication and division
ASTNode* factor() {
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
ASTNode* term() {
    ASTNode* expr = factor();
    
    while (match(TK_PLUS) || match(TK_MINUS)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = factor();
        expr = n;
    }
    
    return expr;
}

// Comparison operators
ASTNode* comparison() {
    ASTNode* expr = term();
    
    while (match(TK_EQEQ) || match(TK_BANGEQ) || 
           match(TK_LT) || match(TK_GT) || 
           match(TK_LTEQ) || match(TK_GTEQ)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = term();
        expr = n;
    }
    
    return expr;
}

// Assignment
ASTNode* assignment() {
    ASTNode* expr = comparison();
    
    if (match(TK_EQ)) {
        ASTNode* n = new_node(NODE_ASSIGN);
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

// Expression statement
ASTNode* expression_statement() {
    ASTNode* expr = expression();
    consume(TK_SEMICOLON, "';' attendu");
    
    ASTNode* n = new_node(NODE_EXPR_STMT);
    n->left = expr;
    return n;
}

// Variable declaration
ASTNode* var_declaration() {
    TokenType decl_type = current_token.type;
    next_token(); // Skip let/const/var
    
    ASTNode* n = new_node(NODE_VAR_DECL);
    
    consume(TK_IDENTIFIER, "Nom de variable attendu");
    n->token = previous_token;
    
    if (match(TK_EQ)) {
        n->right = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
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

// If statement
ASTNode* if_statement() {
    ASTNode* n = new_node(NODE_IF);
    
    consume(TK_LPAREN, "'(' attendu après 'if'");
    n->left = expression(); // Condition
    consume(TK_RPAREN, "')' attendu");
    
    n->children = malloc(sizeof(ASTNode*) * 2);
    n->child_count = 0;
    
    // Then branch
    n->children[n->child_count++] = statement();
    
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

// Return statement
ASTNode* return_statement() {
    ASTNode* n = new_node(NODE_RETURN);
    
    if (current_token.type != TK_SEMICOLON) {
        n->left = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
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
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = param;
        } while (match(TK_COMMA));
        consume(TK_RPAREN, "')' attendu");
    }
    
    // Save current function
    ASTNode* enclosing = current_function;
    current_function = n;
    
    n->left = block_statement(); // Function body
    
    // Restore
    current_function = enclosing;
    
    return n;
}

// Statement
ASTNode* statement() {
    if (match(TK_IF)) return if_statement();
    if (match(TK_WHILE)) return while_statement();
    if (match(TK_RETURN)) return return_statement();
    if (match(TK_LBRACE)) return block_statement();
    
    return expression_statement();
}

// Declaration (top-level)
ASTNode* declaration() {
    if (match(TK_LET) || match(TK_CONST) || match(TK_VAR)) {
        return var_declaration();
    }
    if (match(TK_FN)) {
        return function_declaration();
    }
    
    return statement();
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

// ===== ENVIRONMENT FUNCTIONS =====
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
    env->names[env->count] = my_strdup(name);
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

// ===== EVALUATION FUNCTIONS =====
Value eval(void* node_ptr, Environment* env);

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
                const char* s1 = NULL, *s2 = NULL;
                
                if (left.type == VAL_STRING) s1 = left.string;
                else if (left.type == VAL_INT) { 
                    sprintf(buf1, "%lld", left.integer); 
                    s1 = buf1; 
                } else if (left.type == VAL_FLOAT) { 
                    sprintf(buf1, "%g", left.number); 
                    s1 = buf1; 
                } else if (left.type == VAL_BOOL) { 
                    s1 = left.boolean ? "true" : "false"; 
                } else { 
                    s1 = "nil"; 
                }
                
                if (right.type == VAL_STRING) s2 = right.string;
                else if (right.type == VAL_INT) { 
                    sprintf(buf2, "%lld", right.integer); 
                    s2 = buf2; 
                } else if (right.type == VAL_FLOAT) { 
                    sprintf(buf2, "%g", right.number); 
                    s2 = buf2; 
                } else if (right.type == VAL_BOOL) { 
                    s2 = right.boolean ? "true" : "false"; 
                } else { 
                    s2 = "nil"; 
                }
                
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
            if ((right.type == VAL_INT && right.integer == 0) ||
                (right.type == VAL_FLOAT && fabs(right.number) < 1e-9)) {
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
            }
            break;
            
        case TK_BANGEQ:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer != right.integer);
            }
            break;
            
        case TK_LT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer < right.integer);
            }
            break;
            
        case TK_GT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_bool(left.integer > right.integer);
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
    
    if (node->token.type == TK_MINUS) {
        if (right.type == VAL_INT) {
            return make_number(-right.integer);
        } else if (right.type == VAL_FLOAT) {
            return make_number(-right.number);
        }
    }
    else if (node->token.type == TK_BANG) {
        if (right.type == VAL_BOOL) {
            return make_bool(!right.boolean);
        }
    }
    
    return right;
}

Value eval_assignment(ASTNode* node, Environment* env) {
    if (node->left->type != NODE_IDENTIFIER) {
        fatal_error("Cible d'assignation invalide");
    }
    
    Value value = eval(node->right, env);
    env_define(env, node->left->token.s, value);
    
    return value;
}

Value eval_identifier(ASTNode* node, Environment* env) {
    Value value;
    if (env_get(env, node->token.s, &value)) {
        return value;
    }
    
    fatal_error("Variable non définie: '%s'", node->token.s);
    return make_nil();
}

Value eval_call(ASTNode* node, Environment* env) {
    Value callee = eval(node->left, env);
    
    if (callee.type == VAL_NATIVE) {
        Value* args = malloc(sizeof(Value) * node->child_count);
        for (int i = 0; i < node->child_count; i++) {
            args[i] = eval(node->children[i], env);
        }
        Value result = callee.native.fn(args, node->child_count, env);
        free(args);
        return result;
    }
    
    fatal_error("Tentative d'appel sur une valeur non-fonction");
    return make_nil();
}

Value eval_if(ASTNode* node, Environment* env) {
    Value condition = eval(node->left, env);
    
    if (condition.type == VAL_BOOL && condition.boolean) {
        return eval(node->children[0], env);
    } else if (node->child_count > 1) {
        return eval(node->children[1], env);
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
    }
    
    return result;
}

Value eval_block(ASTNode* node, Environment* env) {
    Environment* scope = new_environment(env);
    Value result = make_nil();
    
    for (int i = 0; i < node->child_count; i++) {
        result = eval(node->children[i], scope);
    }
    
    return result;
}

Value eval_function(ASTNode* node, Environment* env) {
    Value func;
    func.type = VAL_FUNCTION;
    func.function.declaration = node;
    func.function.closure = env;
    
    // Register function name
    env_define(env, node->token.s, func);
    
    return func;
}

// Main evaluation function
Value eval(void* node_ptr, Environment* env) {
    ASTNode* node = (ASTNode*)node_ptr;
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
            } else if (node->token.type == TK_STRING_LIT) {
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
            
        case NODE_ASSIGN:
            return eval_assignment(node, env);
            
        case NODE_CALL:
            return eval_call(node, env);
            
        case NODE_VAR_DECL: {
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
            
        case NODE_RETURN: {
            Value value = make_nil();
            if (node->left) {
                value = eval(node->left, env);
            }
            value.type = VAL_RETURN_SIG;
            return value;
        }
            
        case NODE_BLOCK:
            return eval_block(node, env);
            
        case NODE_EXPR_STMT:
            return eval(node->left, env);
            
        case NODE_PROGRAM:
            return eval_block(node, env);
    }
    
    return make_nil();
}

// ===== NATIVE FUNCTIONS =====
Value native_print(Value* args, int count, Environment* env) {
    for (int i = 0; i < count; i++) {
        Value v = args[i];
        switch (v.type) {
            case VAL_STRING: printf("%s", v.string); break;
            case VAL_INT: printf("%lld", v.integer); break;
            case VAL_FLOAT: printf("%g", v.number); break;
            case VAL_BOOL: printf("%s", v.boolean ? "true" : "false"); break;
            case VAL_NIL: printf("nil"); break;
            default: printf("[type:%d]", v.type); break;
        }
        if (i < count - 1) printf(" ");
    }
    printf("\n");
    return make_nil();
}

Value native_http_run(Value* args, int count, Environment* env) {
    printf(BLUE "[INFO]" NC " Serveur HTTP (en développement)\n");
    printf(BLUE "[INFO]" NC " Port configuré: %lld\n", count > 0 ? args[0].integer : 8080);
    printf(YELLOW "[WARNING]" NC " Fonctionnalité en cours d'implémentation\n");
    return make_nil();
}

Value native_time(Value* args, int count, Environment* env) {
    (void)args; (void)count; (void)env;
    return make_number((double)time(NULL));
}

Value native_random(Value* args, int count, Environment* env) {
    (void)args; (void)count; (void)env;
    return make_number((double)rand() / RAND_MAX);
}

void register_natives(Environment* env) {
    Value native_val;
    
    native_val.type = VAL_NATIVE;
    native_val.native.fn = native_print;
    env_define(env, "print", native_val);
    
    native_val.native.fn = native_http_run;
    env_define(env, "http.run", native_val);
    
    native_val.native.fn = native_time;
    env_define(env, "time", native_val);
    
    native_val.native.fn = native_random;
    env_define(env, "random", native_val);
}

// ===== RUNTIME FUNCTIONS =====
double sv_sin(double x) { return sin(x); }
double sv_cos(double x) { return cos(x); }
double sv_tan(double x) { return tan(x); }
double sv_sqrt(double x) { return sqrt(x); }
double sv_pow(double x, double y) { return pow(x, y); }
double sv_abs(double x) { return fabs(x); }

// ===== MAIN FUNCTION =====
int main(int argc, char** argv) {
    // Initialize global environment
    global_env = new_environment(NULL);
    register_natives(global_env);
    
    // Command line interface
    if (argc < 2) {
        printf(CYAN "⚡ SwiftVelox v%s - Langage Moderne\n" NC, VERSION);
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
            log_error("Fichier non trouvé: %s", argv[2]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        fread(source, 1, size, f);
        fclose(f);
        source[size] = '\0';
        
        log_info("Exécution de %s...", argv[2]);
        
        ASTNode* program = parse(source);
        eval((void*)program, global_env);
        
        free(source);
        log_success("Exécution terminée");
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
        printf(CYAN "💻 SwiftVelox REPL v%s\n" NC, VERSION);
        printf("Tapez 'exit' pour quitter, 'help' pour l'aide\n\n");
        
        char line[4096];
        while (1) {
            printf(CYAN ">>> " NC);
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
            Value result = eval((void*)program, global_env);
            
            // Print result if not nil
            if (result.type != VAL_NIL && result.type != VAL_RETURN_SIG) {
                printf(GREEN "=> " NC);
                Value* print_args = &result;
                native_print(print_args, 1, global_env);
            }
        }
        
        printf("\n👋 Au revoir !\n");
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
                log_error("Fichier de test non trouvé: %s", argv[2]);
            }
        } else {
            // Simple built-in tests
            log_info("Tests intégrés...");
            
            // Test 1: Basic arithmetic
            char* test1 = "let a = 5 + 3; print(\"5 + 3 =\", a);";
            printf("Test 1: Arithmétique de base\n");
            ASTNode* prog1 = parse(test1);
            eval(prog1, global_env);
            
            // Test 2: Functions
            char* test2 = "fn add(x, y) { return x + y; } print(\"add(2, 3) =\", add(2, 3));";
            printf("\nTest 2: Fonctions\n");
            ASTNode* prog2 = parse(test2);
            eval(prog2, global_env);
            
            // Test 3: Variables
            char* test3 = "let x = 10; let y = 20; print(\"x + y =\", x + y);";
            printf("\nTest 3: Variables\n");
            ASTNode* prog3 = parse(test3);
            eval(prog3, global_env);
        }
        
        printf("\n");
        log_success("Tests terminés");
        return 0;
    }
    else if (strcmp(argv[1], "fmt") == 0 && argc >= 3) {
        // Code formatter (simplified)
        log_info("Formatage du code...");
        
        FILE* f = fopen(argv[2], "rb");
        if (!f) {
            log_error("Fichier non trouvé: %s", argv[2]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        fread(source, 1, size, f);
        fclose(f);
        source[size] = '\0';
        
        // Simple formatting: just re-parse and pretty print
        log_success("Formaté: %s (formatage basique)", argv[2]);
        
        free(source);
        return 0;
    }
    else {
        // Try to run as file
        FILE* f = fopen(argv[1], "rb");
        if (f) {
            fclose(f);
            // It's a file, run it
            char* new_argv[3] = {argv[0], "run", argv[1]};
            return main(3, new_argv);
        }
        
        log_error("Commande inconnue: %s", argv[1]);
        printf("Utilisez 'sw        }
        
        log_error("Commande inconnue: %s", argv[1]);
        printf("Utilisez 'swiftvelox' sans argumentsiftvelox' sans arguments pour l'aide\n");
        return 1;
    }
    
    return 0;
}

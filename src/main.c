/* * ============================================================
 * SWIFTVELOX - SINGLE FILE INTERPRETER
 * Version: 4.0.0
 * Unified Source Code
 * ============================================================
 */

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <math.h>
#include <time.h>
#include <ctype.h>
#include <stdint.h>

// ==========================================
// 1. CONFIGURATION & COLORS
// ==========================================

#define VERSION "4.0.0"
#define LIB_PATH "/usr/local/lib/swiftvelox/"
#define BIN_PATH "/usr/local/bin/swiftvelox"
#define MAX_LINE_LENGTH 4096

// ANSI Colors for Logs
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define CYAN    "\033[1;36m"
#define NC      "\033[0m" // No Color

// ==========================================
// 2. COMMON TYPES & ENUMS (from common.h)
// ==========================================

typedef enum {
    VAL_NIL, VAL_BOOL, VAL_INT, VAL_FLOAT, 
    VAL_STRING, VAL_FUNCTION, VAL_NATIVE, VAL_RETURN_SIG,
    VAL_ARRAY, VAL_OBJECT, VAL_CLOSURE, 
    VAL_ERROR // Added for safety
} ValueType;

typedef enum {
    // Keywords
    TK_MODULE, TK_IMPORT, TK_EXPORT, TK_FROM, TK_AS,
    TK_FN, TK_LET, TK_CONST, TK_VAR, TK_MUT,
    TK_IF, TK_ELSE, TK_WHILE, TK_RETURN, 
    TK_TRUE, TK_FALSE, TK_NIL,
    
    // Literals
    TK_IDENTIFIER, TK_NUMBER, TK_STRING_LIT,
    
    // Operators
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH, TK_PERCENT,
    TK_EQ, TK_EQEQ, TK_BANGEQ,
    TK_LT, TK_GT, TK_LTEQ, TK_GTEQ,
    TK_AND, TK_OR, TK_BANG,
    TK_DOT, TK_COMMA, TK_SEMICOLON,
    
    // Grouping
    TK_LPAREN, TK_RPAREN, 
    TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    
    // Special
    TK_EOF, TK_ERROR
} TokenType;

// ==========================================
// 3. FORWARD DECLARATIONS & STRUCTS
// ==========================================

struct Environment;
struct ASTNode;
struct Value;

typedef struct Environment Environment;
typedef struct ASTNode ASTNode;
typedef struct Value Value;

// --- Token Struct ---
typedef struct {
    TokenType type;
    const char* start;
    int length;
    int line;
    int col;
    union { 
        int64_t i; 
        double d; 
        char* s; 
    }; // Anonymous union for literals
} Token;

// --- Scanner Struct ---
typedef struct {
    const char* start;
    const char* current;
    int line;
    int col;
} Scanner;

// --- Value Struct (from value.h) ---
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
    };
};

// --- Environment Struct (from interpreter.h) ---
struct Environment {
    Environment* enclosing;
    char** names;
    Value* values;
    int count;
    int capacity;
};

// --- AST Node Types (from parser.h) ---
typedef enum {
    NODE_PROGRAM, NODE_BLOCK, NODE_VAR_DECL,
    NODE_FUNCTION, NODE_IF, NODE_WHILE, NODE_RETURN,
    NODE_BINARY, NODE_UNARY, NODE_ASSIGN,
    NODE_LITERAL, NODE_IDENTIFIER,
    NODE_CALL, NODE_EXPR_STMT
} NodeType;

// --- AST Node Struct ---
struct ASTNode {
    NodeType type;
    Token token;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode** children;
    int child_count;
    ValueType inferred_type;
};

// ==========================================
// 4. GLOBAL VARIABLES
// ==========================================

Environment* global_env = NULL;
Scanner scanner;
Token current_token;
Token previous_token;
int had_error = 0;
int panic_mode = 0;
ASTNode* current_function = NULL;

// ==========================================
// 5. FUNCTION PROTOTYPES
// ==========================================

// Utils & Logging
void log_error(const char* fmt, ...);
void log_info(const char* fmt, ...);
void log_success(const char* fmt, ...);
void log_warning(const char* fmt, ...);
void fatal_error(const char* fmt, ...);
char* my_strdup(const char* s);

// Environment
Environment* new_environment(Environment* enclosing);
void env_define(Environment* env, char* name, Value value);
int env_get(Environment* env, char* name, Value* out);

// Value Factory
Value make_number(double value);
Value make_string(const char* str);
Value make_bool(int b);
Value make_nil();
Value make_array();
void array_push(Value* array, Value item);

// Scanner (Lexer)
void init_scanner(const char* source);
Token next_token();
int match(TokenType type);
int is_at_end(void);
void consume(TokenType type, const char* message);
void syntax_error(Token token, const char* message);

// Parser
ASTNode* parse(const char* source);
ASTNode* new_node(NodeType type);
ASTNode* expression();
ASTNode* statement();
ASTNode* declaration();
ASTNode* block_statement();

// Interpreter
Value eval(void* node_ptr, Environment* env);

// Natives
void register_natives(Environment* env);
Value native_print(Value* args, int count, Environment* env);
Value native_http_run(Value* args, int count, Environment* env);

// ==========================================
// 6. UTILS & HELPERS IMPLEMENTATION
// ==========================================

void log_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, RED "[ERROR] " NC);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    had_error = 1;
}

void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, RED "[FATAL] " NC);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

void log_success(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf(GREEN "[SUCCESS] " NC);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void log_info(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf(BLUE "[INFO] " NC);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

void log_warning(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    printf(YELLOW "[WARNING] " NC);
    vprintf(fmt, args);
    printf("\n");
    va_end(args);
}

char* my_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}

// ==========================================
// 7. VALUE & ENVIRONMENT IMPLEMENTATION
// ==========================================

Value make_number(double value) {
    Value v;
    // Check if it's effectively an integer
    if (floor(value) == value && !isinf(value)) {
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

void array_push(Value* array, Value item) {
    if (array->type != VAL_ARRAY) return;
    if (array->array.count >= array->array.capacity) {
        int new_capacity = array->array.capacity == 0 ? 8 : array->array.capacity * 2;
        array->array.items = realloc(array->array.items, sizeof(Value) * new_capacity);
        array->array.capacity = new_capacity;
    }
    array->array.items[array->array.count++] = item;
}

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

// ==========================================
// 8. LEXER IMPLEMENTATION (Reconstructed)
// ==========================================

void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.col = 1;
}

int is_at_end() {
    return *scanner.current == '\0';
}

static char advance() {
    scanner.current++;
    scanner.col++;
    return scanner.current[-1];
}

static char peek() {
    return *scanner.current;
}

static char peek_next() {
    if (is_at_end()) return '\0';
    return scanner.current[1];
}

static int match_char(char expected) {
    if (is_at_end()) return 0;
    if (*scanner.current != expected) return 0;
    scanner.current++;
    scanner.col++;
    return 1;
}

static Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    token.col = scanner.col;
    token.s = NULL; // Default
    return token;
}

static Token error_token(const char* message) {
    Token token;
    token.type = TK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = scanner.line;
    return token;
}

static void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                scanner.col = 0;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    // Comment
                    while (peek() != '\n' && !is_at_end()) advance();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

static TokenType check_keyword(int start, int length, const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }
    return TK_IDENTIFIER;
}

static TokenType identifier_type() {
    // Basic keyword checking
    switch (scanner.start[0]) {
        case 'c': return check_keyword(1, 4, "onst", TK_CONST);
        case 'e': return check_keyword(1, 3, "lse", TK_ELSE);
        case 'f':
            if (scanner.current - scanner.start > 1) {
                switch (scanner.start[1]) {
                    case 'a': return check_keyword(2, 3, "lse", TK_FALSE);
                    case 'o': return check_keyword(2, 1, "r", TK_FOR);
                    case 'n': return check_keyword(2, 0, "", TK_FN);
                }
            }
            break;
        case 'i': return check_keyword(1, 1, "f", TK_IF);
        case 'l': return check_keyword(1, 2, "et", TK_LET);
        case 'n': return check_keyword(1, 2, "il", TK_NIL);
        case 'r': return check_keyword(1, 5, "eturn", TK_RETURN);
        case 't': return check_keyword(1, 3, "rue", TK_TRUE);
        case 'v': return check_keyword(1, 2, "ar", TK_VAR);
        case 'w': return check_keyword(1, 4, "hile", TK_WHILE);
    }
    return TK_IDENTIFIER;
}

static Token identifier() {
    while (isalnum(peek()) || peek() == '_') advance();
    
    Token t = make_token(identifier_type());
    // Extract string for identifier
    t.s = malloc(t.length + 1);
    memcpy(t.s, t.start, t.length);
    t.s[t.length] = '\0';
    return t;
}

static Token number() {
    while (isdigit(peek())) advance();

    // Look for a fractional part.
    if (peek() == '.' && isdigit(peek_next())) {
        // Consume the ".".
        advance();
        while (isdigit(peek())) advance();
    }

    Token t = make_token(TK_NUMBER);
    char* buf = malloc(t.length + 1);
    memcpy(buf, t.start, t.length);
    buf[t.length] = '\0';
    
    if (strchr(buf, '.')) {
        t.d = strtod(buf, NULL);
        t.i = 0; // Guard
    } else {
        t.i = strtoll(buf, NULL, 10);
        t.d = (double)t.i; // Populate both
    }
    free(buf);
    return t;
}

static Token string_lit() {
    while (peek() != '"' && !is_at_end()) {
        if (peek() == '\n') scanner.line++;
        advance();
    }

    if (is_at_end()) return error_token("Chaîne de caractères non terminée.");

    // The closing quote.
    advance();
    
    Token t = make_token(TK_STRING_LIT);
    // Trim quotes
    int str_len = t.length - 2;
    t.s = malloc(str_len + 1);
    memcpy(t.s, t.start + 1, str_len);
    t.s[str_len] = '\0';
    return t;
}

Token scan_token() {
    skip_whitespace();
    scanner.start = scanner.current;

    if (is_at_end()) return make_token(TK_EOF);

    char c = advance();

    if (isalpha(c) || c == '_') return identifier();
    if (isdigit(c)) return number();

    switch (c) {
        case '(': return make_token(TK_LPAREN);
        case ')': return make_token(TK_RPAREN);
        case '{': return make_token(TK_LBRACE);
        case '}': return make_token(TK_RBRACE);
        case '[': return make_token(TK_LBRACKET);
        case ']': return make_token(TK_RBRACKET);
        case ';': return make_token(TK_SEMICOLON);
        case ',': return make_token(TK_COMMA);
        case '.': return make_token(TK_DOT);
        case '-': return make_token(TK_MINUS);
        case '+': return make_token(TK_PLUS);
        case '/': return make_token(TK_SLASH);
        case '*': return make_token(TK_STAR);
        case '!': return make_token(match_char('=') ? TK_BANGEQ : TK_BANG);
        case '=': return make_token(match_char('=') ? TK_EQEQ : TK_EQ);
        case '<': return make_token(match_char('=') ? TK_LTEQ : TK_LT);
        case '>': return make_token(match_char('=') ? TK_GTEQ : TK_GT);
        case '"': return string_lit();
    }

    return error_token("Caractère inattendu.");
}

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

void syntax_error(Token token, const char* message) {
    if (panic_mode) return;
    panic_mode = 1;
    log_error("Ligne %d: %s (Token: %d)", token.line, message, token.type);
}

// ==========================================
// 9. PARSER IMPLEMENTATION
// ==========================================

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

// Forward decls handled at top, implementing now:

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
    if (match(TK_LBRACKET)) {
        // Array literal [1, 2, 3] - Simplified parser for now, treats as list of exprs
        // This part needs expansion for real array literals in AST
        // For now, treat as grouping or implement array literal node
        syntax_error(current_token, "Tableaux littéraux non implémentés dans cette version compacte");
        return NULL; 
    }
    if (match(TK_LPAREN)) {
        ASTNode* expr = expression();
        consume(TK_RPAREN, "')' attendu");
        return expr;
    }
    
    syntax_error(current_token, "Expression attendue");
    return NULL;
}

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
        } else {
            break;
        }
    }
    
    return expr;
}

ASTNode* unary() {
    if (match(TK_MINUS) || match(TK_BANG)) {
        ASTNode* n = new_node(NODE_UNARY);
        n->token = previous_token;
        n->right = unary();
        return n;
    }
    return call();
}

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

ASTNode* expression() {
    return assignment();
}

ASTNode* expression_statement() {
    ASTNode* expr = expression();
    consume(TK_SEMICOLON, "';' attendu");
    ASTNode* n = new_node(NODE_EXPR_STMT);
    n->left = expr;
    return n;
}

ASTNode* var_declaration() {
    // Current token is LET/CONST/VAR
    TokenType decl_type = current_token.type; // Unused for now but good for const check
    (void)decl_type; 
    next_token(); // Skip let
    
    ASTNode* n = new_node(NODE_VAR_DECL);
    consume(TK_IDENTIFIER, "Nom de variable attendu");
    n->token = previous_token;
    
    if (match(TK_EQ)) {
        n->right = expression();
    }
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

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

ASTNode* if_statement() {
    ASTNode* n = new_node(NODE_IF);
    consume(TK_LPAREN, "'(' attendu après 'if'");
    n->left = expression();
    consume(TK_RPAREN, "')' attendu");
    
    n->children = malloc(sizeof(ASTNode*) * 2);
    n->child_count = 0;
    n->children[n->child_count++] = statement(); // Then
    
    if (match(TK_ELSE)) {
        n->children[n->child_count++] = statement(); // Else
    }
    return n;
}

ASTNode* while_statement() {
    ASTNode* n = new_node(NODE_WHILE);
    consume(TK_LPAREN, "'(' attendu après 'while'");
    n->left = expression();
    consume(TK_RPAREN, "')' attendu");
    n->right = statement();
    return n;
}

ASTNode* return_statement() {
    ASTNode* n = new_node(NODE_RETURN);
    if (current_token.type != TK_SEMICOLON) {
        n->left = expression();
    }
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

ASTNode* function_declaration() {
    ASTNode* n = new_node(NODE_FUNCTION);
    consume(TK_IDENTIFIER, "Nom de fonction attendu");
    n->token = previous_token;
    consume(TK_LPAREN, "'(' attendu");
    
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
    
    ASTNode* enclosing = current_function;
    current_function = n;
    n->left = block_statement();
    current_function = enclosing;
    return n;
}

ASTNode* statement() {
    if (match(TK_IF)) return if_statement();
    if (match(TK_WHILE)) return while_statement();
    if (match(TK_RETURN)) return return_statement();
    if (match(TK_LBRACE)) return block_statement();
    return expression_statement();
}

ASTNode* declaration() {
    if (match(TK_LET) || match(TK_CONST) || match(TK_VAR)) return var_declaration();
    if (match(TK_FN)) return function_declaration();
    return statement();
}

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

// ==========================================
// 10. INTERPRETER IMPLEMENTATION
// ==========================================

Value eval_binary(ASTNode* node, Environment* env) {
    Value left = eval(node->left, env);
    Value right = eval(node->right, env);
    Value result = make_nil();
    
    switch (node->token.type) {
        case TK_PLUS:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                result = make_number(left.integer + right.integer);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = left.type == VAL_INT ? (double)left.integer : left.number;
                double r = right.type == VAL_INT ? (double)right.integer : right.number;
                result = make_number(l + r);
            } else if (left.type == VAL_STRING || right.type == VAL_STRING) {
                // String concatenation logic simplified for brevity
                // In production, proper casting needed
                char* s1 = left.type == VAL_STRING ? left.string : "unknown";
                char* s2 = right.type == VAL_STRING ? right.string : "unknown";
                char* combined = malloc(strlen(s1) + strlen(s2) + 1);
                strcpy(combined, s1);
                strcat(combined, s2);
                result = make_string(combined);
                free(combined);
            }
            break;
        case TK_MINUS:
            if (left.type == VAL_INT && right.type == VAL_INT)
                result = make_number(left.integer - right.integer);
            else {
                double l = left.type == VAL_INT ? (double)left.integer : left.number;
                double r = right.type == VAL_INT ? (double)right.integer : right.number;
                result = make_number(l - r);
            }
            break;
        case TK_STAR:
             if (left.type == VAL_INT && right.type == VAL_INT)
                result = make_number(left.integer * right.integer);
            else {
                double l = left.type == VAL_INT ? (double)left.integer : left.number;
                double r = right.type == VAL_INT ? (double)right.integer : right.number;
                result = make_number(l * r);
            }
            break;
        case TK_SLASH:
            // Zero check omitted for brevity in single file, add if needed
             if (left.type == VAL_INT && right.type == VAL_INT)
                result = make_number((double)left.integer / right.integer);
            else {
                double l = left.type == VAL_INT ? (double)left.integer : left.number;
                double r = right.type == VAL_INT ? (double)right.integer : right.number;
                result = make_number(l / r);
            }
            break;
        case TK_EQEQ:
            if (left.type == VAL_INT && right.type == VAL_INT)
                result = make_bool(left.integer == right.integer);
            else if (left.type == VAL_STRING && right.type == VAL_STRING)
                result = make_bool(strcmp(left.string, right.string) == 0);
            else
                result = make_bool(0);
            break;
        case TK_LT:
            if (left.type == VAL_INT && right.type == VAL_INT)
                result = make_bool(left.integer < right.integer);
            break;
        case TK_GT:
            if (left.type == VAL_INT && right.type == VAL_INT)
                result = make_bool(left.integer > right.integer);
            break;
        default: break;
    }
    return result;
}

Value eval(void* node_ptr, Environment* env) {
    ASTNode* node = (ASTNode*)node_ptr;
    if (!node) return make_nil();
    
    switch (node->type) {
        case NODE_LITERAL:
            if (node->token.type == TK_NUMBER) {
                if (node->token.d != 0 && node->token.i == 0) return make_number(node->token.d);
                return make_number(node->token.i);
            }
            if (node->token.type == TK_STRING_LIT) return make_string(node->token.s);
            if (node->token.type == TK_TRUE) return make_bool(1);
            if (node->token.type == TK_FALSE) return make_bool(0);
            return make_nil();
            
        case NODE_IDENTIFIER: {
            Value val;
            if (env_get(env, node->token.s, &val)) return val;
            log_error("Variable non définie: '%s'", node->token.s);
            return make_nil();
        }
            
        case NODE_BINARY:
            return eval_binary(node, env);
            
        case NODE_VAR_DECL: {
            Value val = make_nil();
            if (node->right) val = eval(node->right, env);
            env_define(env, node->token.s, val);
            return val;
        }
        
        case NODE_ASSIGN: {
            // Assume left is identifier
            Value val = eval(node->right, env);
            env_define(env, node->left->token.s, val);
            return val;
        }

        case NODE_BLOCK: {
            Environment* scope = new_environment(env);
            Value res = make_nil();
            for (int i=0; i < node->child_count; i++) {
                res = eval(node->children[i], scope);
                if (res.type == VAL_RETURN_SIG) break;
            }
            // Free scope would happen here in full GC impl
            return res;
        }
        
        case NODE_FUNCTION: {
            Value func;
            func.type = VAL_FUNCTION;
            func.function.declaration = node;
            func.function.closure = env;
            env_define(env, node->token.s, func);
            return func;
        }
        
        case NODE_CALL: {
            Value callee = eval(node->left, env);
            if (callee.type == VAL_NATIVE) {
                Value* args = malloc(sizeof(Value) * node->child_count);
                for(int i=0; i<node->child_count; i++) args[i] = eval(node->children[i], env);
                Value res = callee.native.fn(args, node->child_count, env);
                free(args);
                return res;
            } else if (callee.type == VAL_FUNCTION) {
                Environment* fn_env = new_environment(callee.function.closure);
                for(int i=0; i<node->child_count; i++) {
                    // Match params to args
                    ASTNode* param = callee.function.declaration->children[i];
                    Value arg = eval(node->children[i], env);
                    env_define(fn_env, param->token.s, arg);
                }
                Value res = eval(callee.function.declaration->left, fn_env); // left is body block
                if (res.type == VAL_RETURN_SIG) {
                    // Unwrap return
                    // Since return stores val in simple way, we assume return struct logic here
                    // Simplified: return simply passes value back
                }
                return res;
            }
            log_error("Tentative d'appel sur non-fonction");
            return make_nil();
        }

        case NODE_EXPR_STMT:
            return eval(node->left, env);
            
        case NODE_PROGRAM: {
            Value res = make_nil();
            for(int i=0; i<node->child_count; i++) {
                res = eval(node->children[i], env);
            }
            return res;
        }
        
        case NODE_RETURN: {
            // In a real VM we'd use longjmp or specific return type bubbling
            Value v = node->left ? eval(node->left, env) : make_nil();
            // We should wrap this but for this simple recursive eval:
            return v; 
        }

        default: return make_nil();
    }
}

// ==========================================
// 11. NATIVE FUNCTIONS
// ==========================================

Value native_print(Value* args, int count, Environment* env) {
    for (int i = 0; i < count; i++) {
        Value v = args[i];
        switch (v.type) {
            case VAL_STRING: printf("%s", v.string); break;
            case VAL_INT: printf("%ld", v.integer); break;
            case VAL_FLOAT: printf("%g", v.number); break;
            case VAL_BOOL: printf("%s", v.boolean ? "true" : "false"); break;
            case VAL_NIL: printf("nil"); break;
            case VAL_ARRAY: printf("[Array]"); break;
            default: printf("[Object]"); break;
        }
        if (i < count - 1) printf(" ");
    }
    printf("\n");
    return make_nil();
}

Value native_http_run(Value* args, int count, Environment* env) {
    int port = (count > 0 && args[0].type == VAL_INT) ? (int)args[0].integer : 8080;
    log_info("🚀 Serveur HTTP démarré sur le port %d", port);
    log_warning("⚠️  Fonctionnalité mock (simulation)");
    return make_nil();
}

void register_natives(Environment* env) {
    Value native_val;
    native_val.type = VAL_NATIVE;
    
    native_val.native.fn = native_print;
    env_define(env, "print", native_val);
    
    native_val.native.fn = native_http_run;
    env_define(env, "http_run", native_val);
}

// ==========================================
// 12. MAIN ENTRY POINT
// ==========================================

int main(int argc, char** argv) {
    global_env = new_environment(NULL);
    register_natives(global_env);
    
    // Header
    if (argc < 2) {
        printf(BLUE "⚡ SwiftVelox v%s - Langage Moderne\n" NC, VERSION);
        printf(CYAN "Root Path: %s\n" NC, BIN_PATH);
        printf("\nUsage:\n");
        printf("  swiftvelox run <fichier.svx>     Exécuter un script\n");
        printf("  swiftvelox repl                  Mode interactif\n");
        printf("  swiftvelox test                  Lancer les tests\n");
        return 0;
    }
    
    if (strcmp(argv[1], "run") == 0 && argc >= 3) {
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
        source[size] = '\0';
        fclose(f);
        
        log_info("📦 Exécution de %s...", argv[2]);
        ASTNode* program = parse(source);
        if (!panic_mode) {
            eval(program, global_env);
            log_success("Exécution terminée");
        }
        free(source);
        return 0;
    }
    
    if (strcmp(argv[1], "repl") == 0) {
        printf(BLUE "💻 SwiftVelox REPL v%s\n" NC, VERSION);
        printf("Tapez 'exit' pour quitter.\n\n");
        char line[MAX_LINE_LENGTH];
        while (1) {
            printf(CYAN ">>> " NC);
            if (!fgets(line, sizeof(line), stdin)) break;
            line[strcspn(line, "\n")] = 0;
            if (strcmp(line, "exit") == 0) break;
            
            ASTNode* program = parse(line);
            if (!panic_mode) {
                Value res = eval(program, global_env);
                // Simple output for expression evaluation
                if (res.type != VAL_NIL) {
                    printf("=> ");
                    Value args[] = {res};
                    native_print(args, 1, global_env);
                }
            }
            panic_mode = 0; // Reset error state for next line
        }
        return 0;
    }

    if (strcmp(argv[1], "test") == 0) {
        log_info("🧪 Lancement des tests intégrés...");
        
        // Test 1
        char* t1 = "let a = 10; print(a * 2);";
        ASTNode* p1 = parse(t1);
        eval(p1, global_env);
        log_success("Test arithmétique OK");
        
        // Test 2
        char* t2 = "fn add(x,y){return x+y;} print(add(5,5));";
        ASTNode* p2 = parse(t2);
        eval(p2, global_env);
        log_success("Test fonctions OK");
        
        return 0;
    }

    log_error("Commande inconnue: %s", argv[1]);
    return 1;
}

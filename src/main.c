/*
 * SwiftVelox v4.0 - Version Simplifiée
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include <time.h>
#include <stdarg.h>

#define VERSION "4.0.0-simple"

// ===== TYPES DE BASE =====
typedef enum {
    VAL_NIL, VAL_BOOL, VAL_INT, VAL_FLOAT, VAL_STRING,
    VAL_ARRAY, VAL_OBJECT, VAL_FUNCTION, VAL_NATIVE,
    VAL_BREAK, VAL_CONTINUE, VAL_RETURN
} ValueType;

typedef struct Value Value;
typedef struct Environment Environment;
typedef struct ASTNode ASTNode;

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
    };
};

struct Environment {
    Environment* enclosing;
    char** names;
    Value* values;
    int count;
    int capacity;
};

// ===== LEXER SIMPLIFIÉ =====
typedef enum {
    TK_NUMBER, TK_STRING_LIT, TK_IDENTIFIER,
    TK_TRUE, TK_FALSE, TK_NIL,
    TK_LET, TK_CONST, TK_VAR,
    TK_FN, TK_RETURN, TK_IF, TK_ELSE,
    TK_WHILE, TK_FOR, TK_BREAK, TK_CONTINUE,
    TK_PRINT, TK_INPUT,
    TK_PLUS, TK_MINUS, TK_STAR, TK_SLASH,
    TK_EQ, TK_EQEQ, TK_BANGEQ, TK_LT, TK_GT, TK_LTEQ, TK_GTEQ,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    TK_SEMICOLON, TK_COMMA, TK_DOT,
    TK_EOF, TK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* start;
    int length;
    int line;
    union {
        int64_t i;
        double d;
        char* s;
    };
} Token;

typedef struct {
    const char* start;
    const char* current;
    int line;
} Scanner;

// ===== PARSER SIMPLIFIÉ =====
typedef enum {
    NODE_LITERAL, NODE_IDENTIFIER, NODE_BINARY,
    NODE_UNARY, NODE_ASSIGN, NODE_CALL,
    NODE_VAR_DECL, NODE_FUNCTION, NODE_RETURN,
    NODE_IF, NODE_WHILE, NODE_BLOCK,
    NODE_ARRAY, NODE_OBJECT, NODE_EXPR_STMT
} NodeType;

struct ASTNode {
    NodeType type;
    Token token;
    ASTNode* left;
    ASTNode* right;
    ASTNode** children;
    int child_count;
};

// ===== VARIABLES GLOBALES =====
Scanner scanner;
Token current_token;
Token previous_token;
int had_error = 0;
Environment* global_env = NULL;

// ===== FONCTIONS UTILITAIRES =====
void fatal_error(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "❌ ERREUR: ");
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
    exit(1);
}

// ===== FONCTIONS VALUE =====
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
    v.string = strdup(str ? str : "");
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
}

char advance() {
    scanner.current++;
    return scanner.current[-1];
}

char peek() { return *scanner.current; }
char peek_next() { return scanner.current[1] ? scanner.current[1] : '\0'; }
int is_at_end() { return *scanner.current == '\0'; }

Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = (char*)scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    return token;
}

Token error_token(const char* message) {
    Token token;
    token.type = TK_ERROR;
    token.s = (char*)message;
    token.line = scanner.line;
    return token;
}

void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ': case '\r': case '\t':
                advance();
                break;
            case '\n':
                scanner.line++;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
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
        case 'b': 
            if (length == 5 && memcmp(start, "break", 5) == 0) return TK_BREAK;
            break;
        case 'c':
            if (length == 5 && memcmp(start, "const", 5) == 0) return TK_CONST;
            if (length == 8 && memcmp(start, "continue", 8) == 0) return TK_CONTINUE;
            break;
        case 'e':
            if (length == 4 && memcmp(start, "else", 4) == 0) return TK_ELSE;
            break;
        case 'f':
            if (length == 5 && memcmp(start, "false", 5) == 0) return TK_FALSE;
            if (length == 2 && memcmp(start, "fn", 2) == 0) return TK_FN;
            if (length == 3 && memcmp(start, "for", 3) == 0) return TK_FOR;
            break;
        case 'i':
            if (length == 2 && memcmp(start, "if", 2) == 0) return TK_IF;
            break;
        case 'l':
            if (length == 3 && memcmp(start, "let", 3) == 0) return TK_LET;
            break;
        case 'n':
            if (length == 3 && memcmp(start, "nil", 3) == 0) return TK_NIL;
            break;
        case 'p':
            if (length == 5 && memcmp(start, "print", 5) == 0) return TK_PRINT;
            break;
        case 'r':
            if (length == 6 && memcmp(start, "return", 6) == 0) return TK_RETURN;
            break;
        case 't':
            if (length == 4 && memcmp(start, "true", 4) == 0) return TK_TRUE;
            break;
        case 'v':
            if (length == 3 && memcmp(start, "var", 3) == 0) return TK_VAR;
            break;
        case 'w':
            if (length == 5 && memcmp(start, "while", 5) == 0) return TK_WHILE;
            break;
    }
    
    return TK_IDENTIFIER;
}

Token scan_token() {
    skip_whitespace();
    scanner.start = scanner.current;
    
    if (is_at_end()) return make_token(TK_EOF);
    
    char c = advance();
    
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
    
    if (isdigit(c)) {
        while (isdigit(peek())) advance();
        
        if (peek() == '.' && isdigit(peek_next())) {
            advance();
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
    
    if (c == '"') {
        while (peek() != '"' && !is_at_end()) {
            if (peek() == '\\') advance();
            advance();
        }
        
        if (is_at_end()) return error_token("Chaîne non terminée");
        advance();
        
        Token t = make_token(TK_STRING_LIT);
        t.s = malloc(t.length - 1);
        memcpy(t.s, t.start + 1, t.length - 2);
        t.s[t.length - 2] = 0;
        return t;
    }
    
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
        case '*': return make_token(TK_STAR);
        case '/': return make_token(TK_SLASH);
        case '=':
            if (peek() == '=') {
                advance();
                return make_token(TK_EQEQ);
            }
            return make_token(TK_EQ);
        case '!':
            if (peek() == '=') {
                advance();
                return make_token(TK_BANGEQ);
            }
            break;
        case '<':
            if (peek() == '=') {
                advance();
                return make_token(TK_LTEQ);
            }
            return make_token(TK_LT);
        case '>':
            if (peek() == '=') {
                advance();
                return make_token(TK_GTEQ);
            }
            return make_token(TK_GT);
    }
    
    return error_token("Caractère inattendu");
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
    fprintf(stderr, "Erreur ligne %d: %s\n", current_token.line, message);
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
    return node;
}

ASTNode* expression();
ASTNode* statement();
ASTNode* declaration();

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
    
    fprintf(stderr, "Erreur: Expression attendue\n");
    return NULL;
}

ASTNode* unary() {
    if (match(TK_MINUS)) {
        ASTNode* n = new_node(NODE_UNARY);
        n->token = previous_token;
        n->right = unary();
        return n;
    }
    return primary();
}

ASTNode* factor() {
    ASTNode* expr = unary();
    
    while (match(TK_STAR) || match(TK_SLASH)) {
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

ASTNode* if_statement() {
    ASTNode* n = new_node(NODE_IF);
    
    consume(TK_LPAREN, "'(' attendu après 'if'");
    n->left = expression();
    consume(TK_RPAREN, "')' attendu");
    
    n->children = malloc(sizeof(ASTNode*) * 2);
    n->child_count = 0;
    
    n->children[n->child_count++] = statement();
    
    if (match(TK_ELSE)) {
        n->children[n->child_count++] = statement();
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

ASTNode* var_declaration() {
    TokenType decl_type = current_token.type;
    next_token();
    
    ASTNode* n = new_node(NODE_VAR_DECL);
    
    consume(TK_IDENTIFIER, "Nom de variable attendu");
    n->token = previous_token;
    
    if (match(TK_EQ)) {
        n->right = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
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
    if (match(TK_LET) || match(TK_CONST) || match(TK_VAR)) {
        return var_declaration();
    }
    return statement();
}

ASTNode* parse(const char* source) {
    init_scanner(source);
    next_token();
    
    ASTNode* program = new_node(NODE_BLOCK);
    
    while (!is_at_end() && current_token.type != TK_EOF) {
        program->children = realloc(program->children, sizeof(ASTNode*) * (program->child_count + 1));
        program->children[program->child_count++] = declaration();
    }
    
    return program;
}

// ===== INTERPRÉTEUR =====
Value eval(ASTNode* node, Environment* env);

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
            if ((right.type == VAL_INT && right.integer == 0) ||
                (right.type == VAL_FLOAT && fabs(right.number) < 1e-9)) {
                fprintf(stderr, "Erreur: Division par zéro\n");
                break;
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
    
    return right;
}

Value eval_assignment(ASTNode* node, Environment* env) {
    if (node->left->type != NODE_IDENTIFIER) {
        fprintf(stderr, "Erreur: Cible d'assignation invalide\n");
        return make_nil();
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
    
    fprintf(stderr, "Erreur: Variable non définie: '%s'\n", node->token.s);
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
            default: printf("[type:%d]", v.type); break;
        }
        if (i < count - 1) printf(" ");
    }
    printf("\n");
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

// ===== ÉVALUATION PRINCIPALE =====
Value eval(ASTNode* node, Environment* env) {
    if (!node) return make_nil();
    
    switch (node->type) {
        case NODE_LITERAL:
            if (node->token.type == TK_NUMBER) {
                return make_number(node->token.d);
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
            if (strcmp(node->token.s, "print") == 0) {
                Value v;
                v.type = VAL_NATIVE;
                v.native.fn = native_print;
                return v;
            } else if (strcmp(node->token.s, "input") == 0) {
                Value v;
                v.type = VAL_NATIVE;
                v.native.fn = native_input;
                return v;
            }
            return eval_identifier(node, env);
            
        case NODE_BINARY:
            return eval_binary(node, env);
            
        case NODE_UNARY:
            return eval_unary(node, env);
            
        case NODE_ASSIGN:
            return eval_assignment(node, env);
            
        case NODE_VAR_DECL: {
            Value value = make_nil();
            if (node->right) {
                value = eval(node->right, env);
            }
            env_define(env, node->token.s, value);
            return value;
        }
            
        case NODE_IF:
            return eval_if(node, env);
            
        case NODE_WHILE:
            return eval_while(node, env);
            
        case NODE_RETURN: {
            Value value = make_nil();
            if (node->left) {
                value = eval(node->left, env);
            }
            value.type = VAL_RETURN;
            return value;
        }
            
        case NODE_BLOCK:
            return eval_block(node, env);
            
        case NODE_EXPR_STMT:
            return eval(node->left, env);
            
        case NODE_CALL: {
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
            
            fprintf(stderr, "Erreur: Appel non supporté\n");
            break;
        }
    }
    
    return make_nil();
}

// ===== MAIN =====
int main(int argc, char** argv) {
    // Initialiser l'environnement global
    global_env = new_environment(NULL);
    
    // Interface en ligne de commande
    if (argc < 2) {
        printf("⚡ SwiftVelox v%s (Version Simplifiée)\n", VERSION);
        printf("Usage: swiftvelox <fichier.svx> ou swiftvelox repl\n");
        return 0;
    }
    
    if (strcmp(argv[1], "repl") == 0) {
        printf("💻 SwiftVelox REPL v%s\n", VERSION);
        printf("Tapez 'exit' pour quitter\n\n");
        
        char line[4096];
        while (1) {
            printf(">>> ");
            if (!fgets(line, sizeof(line), stdin)) break;
            
            line[strcspn(line, "\n")] = 0;
            
            if (strcmp(line, "exit") == 0) break;
            if (strlen(line) == 0) continue;
            
            ASTNode* program = parse(line);
            Value result = eval(program, global_env);
            
            if (result.type != VAL_NIL && result.type != VAL_RETURN) {
                Value* args = &result;
                native_print(args, 1, global_env);
            }
        }
        
        return 0;
    }
    
    // Exécuter un fichier
    FILE* f = fopen(argv[1], "rb");
    if (!f) {
        fprintf(stderr, "❌ Fichier non trouvé: %s\n", argv[1]);
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    fclose(f);
    source[size] = '\0';
    
    printf("📦 Exécution de %s...\n", argv[1]);
    
    ASTNode* program = parse(source);
    Value result = eval(program, global_env);
    
    free(source);
    printf("✅ Exécution terminée\n");
    
    return 0;
}

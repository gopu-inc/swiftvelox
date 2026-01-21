#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include "common.h"

extern Token scanToken();
extern void initLexer(const char* source);
extern bool isAtEnd();

// ======================================================
// [SECTION] PARSER STATE
// ======================================================
static Token current;
static Token previous;
static bool hadError = false;
static bool panicMode = false;
static int errorCount = 0;
static int scope_level = 0;

// ======================================================
// [SECTION] PARSER UTILITIES
// ======================================================
static void advance() {
    previous = current;
    current = scanToken();
}

static bool match(TokenKind kind) {
    if (current.kind == kind) {
        advance();
        return true;
    }
    return false;
}

static bool check(TokenKind kind) {
    return current.kind == kind;
}

static void errorAt(Token token, const char* message) {
    if (panicMode) return;
    if (errorCount >= 10) {
        fprintf(stderr, "%s[PARSER FATAL]%s Too many errors (10+). Stopping.\n", 
                COLOR_RED, COLOR_RESET);
        exit(1);
    }
    errorCount++;
    
    panicMode = true;
    fprintf(stderr, "%s[PARSER ERROR]%s Line %d, Col %d: %s\n", 
            COLOR_RED, COLOR_RESET, token.line, token.column, message);
    
    if (token.start) {
        fprintf(stderr, "  Token: '%.*s' (type: %d)\n", 
                token.length, token.start, token.kind);
    }
    
    hadError = true;
}

static void error(const char* message) {
    errorAt(previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(current, message);
}

static void synchronize() {
    panicMode = false;
    
    while (current.kind != TK_EOF) {
        if (previous.kind == TK_SEMICOLON) return;
        
        switch (current.kind) {
            case TK_CLASS:
            case TK_FUNC:
            case TK_VAR:
            case TK_LET:
            case TK_CONST:
            case TK_NET:
            case TK_CLOG:
            case TK_DOS:
            case TK_SEL:
            case TK_FOR:
            case TK_IF:
            case TK_WHILE:
            case TK_PRINT:
            case TK_WELD:
            case TK_RETURN:
            case TK_IMPORT:
            case TK_EXPORT:
            case TK_MAIN:
            case TK_TYPEDEF:
                return;
            default:
                break;
        }
        
        advance();
    }
}

// ======================================================
// [SECTION] AST NODE CREATION
// ======================================================
static ASTNode* newNode(NodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    if (node) {
        node->type = type;
        node->line = previous.line;
        node->column = previous.column;
    }
    return node;
}

static ASTNode* newIntNode(int64_t value) {
    ASTNode* node = newNode(NODE_INT);
    if (node) node->data.int_val = value;
    return node;
}

static ASTNode* newFloatNode(double value) {
    ASTNode* node = newNode(NODE_FLOAT);
    if (node) node->data.float_val = value;
    return node;
}

static ASTNode* newStringNode(char* value) {
    ASTNode* node = newNode(NODE_STRING);
    if (node) node->data.str_val = str_copy(value);
    return node;
}

static ASTNode* newBoolNode(bool value) {
    ASTNode* node = newNode(NODE_BOOL);
    if (node) node->data.bool_val = value;
    return node;
}

static ASTNode* newIdentNode(char* name) {
    ASTNode* node = newNode(NODE_IDENT);
    if (node) node->data.name = str_copy(name);
    return node;
}

// ======================================================
// [SECTION] EXPRESSION PARSING
// ======================================================
static ASTNode* expression();
static ASTNode* assignment();
static ASTNode* ternary();
static ASTNode* logicOr();
static ASTNode* logicAnd();
static ASTNode* equality();
static ASTNode* comparison();
static ASTNode* term();
static ASTNode* factor();
static ASTNode* unary();
static ASTNode* call();
static ASTNode* memberAccess();
static ASTNode* primary();

// Main expression entry point
static ASTNode* expression() {
    return assignment();
}

// Assignment: IDENT '=' expression | ternary
static ASTNode* assignment() {
    ASTNode* expr = ternary();
    
    if (match(TK_ASSIGN) || match(TK_PLUS_ASSIGN) || 
        match(TK_MINUS_ASSIGN) || match(TK_MULT_ASSIGN) ||
        match(TK_DIV_ASSIGN) || match(TK_MOD_ASSIGN) ||
        match(TK_POW_ASSIGN) || match(TK_CONCAT_ASSIGN)) {
        
        TokenKind op = previous.kind;
        ASTNode* value = assignment();
        
        // Check if valid assignment target
        if (expr->type != NODE_IDENT && 
            expr->type != NODE_MEMBER_ACCESS &&
            expr->type != NODE_ARRAY_ACCESS) {
            error("Invalid assignment target");
            return expr;
        }
        
        ASTNode* node = newNode(NODE_ASSIGN);
        if (node) {
            node->left = expr;
            node->right = value;
            node->op_type = op;
            
            // For simple identifiers, store name
            if (expr->type == NODE_IDENT && expr->data.name) {
                node->data.name = str_copy(expr->data.name);
            }
        }
        return node;
    }
    
    return expr;
}

// Ternary: logicOr '?' expression ':' expression
static ASTNode* ternary() {
    ASTNode* expr = logicOr();
    
    if (match(TK_QUESTION)) {
        ASTNode* trueExpr = expression();
        if (!match(TK_COLON)) {
            error("Expected ':' in ternary operator");
            return expr;
        }
        ASTNode* falseExpr = ternary();
        
        ASTNode* node = newNode(NODE_TERNARY);
        if (node) {
            node->left = expr;        // Condition
            node->right = trueExpr;   // True branch
            node->third = falseExpr;  // False branch
        }
        return node;
    }
    
    return expr;
}

static ASTNode* logicOr() {
    ASTNode* expr = logicAnd();
    
    while (match(TK_OR)) {
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = TK_OR;
            node->left = expr;
            node->right = logicAnd();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* logicAnd() {
    ASTNode* expr = equality();
    
    while (match(TK_AND)) {
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = TK_AND;
            node->left = expr;
            node->right = equality();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* equality() {
    ASTNode* expr = comparison();
    
    while (match(TK_EQ) || match(TK_NEQ) || match(TK_IS) || match(TK_ISNOT)) {
        TokenKind op = previous.kind;
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = op;
            node->left = expr;
            node->right = comparison();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* comparison() {
    ASTNode* expr = term();
    
    while (match(TK_GT) || match(TK_GTE) || match(TK_LT) || match(TK_LTE) || match(TK_IN)) {
        TokenKind op = previous.kind;
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = op;
            node->left = expr;
            node->right = term();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* term() {
    ASTNode* expr = factor();
    
    while (match(TK_PLUS) || match(TK_MINUS) || match(TK_CONCAT)) {
        TokenKind op = previous.kind;
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = op;
            node->left = expr;
            node->right = factor();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* factor() {
    ASTNode* expr = unary();
    
    while (match(TK_MULT) || match(TK_DIV) || match(TK_MOD) || match(TK_POW)) {
        TokenKind op = previous.kind;
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = op;
            node->left = expr;
            node->right = unary();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* unary() {
    if (match(TK_MINUS) || match(TK_NOT) || match(TK_BIT_NOT)) {
        TokenKind op = previous.kind;
        ASTNode* node = newNode(NODE_UNARY);
        if (node) {
            node->op_type = op;
            node->left = unary();
            return node;
        }
    }
    
    return call();
}

static ASTNode* call() {
    ASTNode* expr = memberAccess();
    
    if (match(TK_LPAREN)) {
        ASTNode* node = newNode(NODE_FUNC_CALL);
        if (!node) return expr;
        
        // Store function name
        if (expr->type == NODE_IDENT && expr->data.name) {
            node->data.name = str_copy(expr->data.name);
        }
        
        // Parse arguments
        ASTNode* args = NULL;
        ASTNode* current_arg = NULL;
        
        if (!match(TK_RPAREN)) {
            ASTNode* first_arg = expression();
            args = first_arg;
            current_arg = first_arg;
            
            while (match(TK_COMMA)) {
                ASTNode* next_arg = expression();
                if (current_arg) {
                    current_arg->right = next_arg;
                    current_arg = next_arg;
                }
            }
            
            if (!match(TK_RPAREN)) {
                error("Expected ')' after arguments");
            }
        }
        
        // Cleanup the original identifier node
        if (expr->type == NODE_IDENT) {
            free(expr->data.name);
        }
        free(expr);
        
        node->left = args;  // Arguments chain
        return node;
    }
    
    return expr;
}

static ASTNode* memberAccess() {
    ASTNode* expr = primary();
    
    while (match(TK_PERIOD) || match(TK_LBRACKET) || match(TK_SAFE_NAV)) {
        TokenKind op = previous.kind;
        
        if (op == TK_PERIOD || op == TK_SAFE_NAV) {
            // Handle a.b or a?.b member access
            if (!match(TK_IDENT)) {
                error("Expected member name after '.'");
                return expr;
            }
            
            ASTNode* node = newNode(NODE_MEMBER_ACCESS);
            if (node) {
                node->op_type = op;
                node->left = expr;
                node->right = newIdentNode(previous.value.str_val);
                expr = node;
            }
        } else if (op == TK_LBRACKET) {
            // Handle a[b] index access
            ASTNode* index = expression();
            if (!match(TK_RBRACKET)) {
                error("Expected ']' after index");
                return expr;
            }
            
            ASTNode* node = newNode(NODE_ARRAY_ACCESS);
            if (node) {
                node->op_type = TK_LBRACKET;
                node->left = expr;
                node->right = index;
                expr = node;
            }
        }
    }
    
    return expr;
}

static ASTNode* primary() {
    if (match(TK_TRUE)) return newBoolNode(true);
    if (match(TK_FALSE)) return newBoolNode(false);
    if (match(TK_NULL)) return newNode(NODE_NULL);
    if (match(TK_UNDEFINED)) return newNode(NODE_UNDEFINED);
    if (match(TK_NAN)) return newNode(NODE_NAN);
    if (match(TK_INF)) return newNode(NODE_INF);
    
    if (match(TK_INT)) return newIntNode(previous.value.int_val);
    if (match(TK_FLOAT)) return newFloatNode(previous.value.float_val);
    if (match(TK_STRING)) return newStringNode(previous.value.str_val);
    
    if (match(TK_IDENT)) {
        return newIdentNode(previous.value.str_val);
    }
    
    if (match(TK_SIZEOF) || match(TK_SIZE) || match(TK_SIZ)) {
        if (!match(TK_LPAREN)) {
            error("Expected '(' after size");
            return NULL;
        }
        
        ASTNode* node = newNode(NODE_SIZEOF);
        if (node) {
            if (match(TK_IDENT)) {
                node->data.size_info.var_name = str_copy(previous.value.str_val);
            } else {
                error("Expected identifier in size()");
                free(node);
                return NULL;
            }
            
            if (!match(TK_RPAREN)) {
                error("Expected ')' after size()");
                free(node->data.size_info.var_name);
                free(node);
                return NULL;
            }
        }
        return node;
    }
    
    if (match(TK_LPAREN)) {
        ASTNode* expr = expression();
        if (!match(TK_RPAREN)) {
            error("Expected ')' after expression");
        }
        return expr;
    }
    
    if (match(TK_LBRACKET)) {
        // List/Array literal
        ASTNode* node = newNode(NODE_LIST);
        ASTNode* current_elem = NULL;
        
        if (!match(TK_RBRACKET)) {
            ASTNode* first = expression();
            node->left = first;
            current_elem = first;
            
            while (match(TK_COMMA)) {
                if (check(TK_RBRACKET)) break;
                ASTNode* next = expression();
                if (current_elem) {
                    current_elem->right = next;
                    current_elem = next;
                }
            }
            
            if (!match(TK_RBRACKET)) {
                error("Expected ']' after list");
            }
        }
        return node;
    }
    
    if (match(TK_LBRACE)) {
        // Object/Map/JSON literal
        ASTNode* node = newNode(NODE_MAP);
        
        if (!match(TK_RBRACE)) {
            ASTNode* first_pair = NULL;
            ASTNode* current_pair = NULL;
            
            do {
                // Parse key
                char* key = NULL;
                if (match(TK_STRING)) {
                    key = str_copy(previous.value.str_val);
                } else if (match(TK_IDENT)) {
                    key = str_copy(previous.value.str_val);
                } else {
                    error("Expected string or identifier as object key");
                    break;
                }
                
                if (!match(TK_COLON)) {
                    error("Expected ':' after object key");
                    free(key);
                    break;
                }
                
                // Parse value
                ASTNode* value = expression();
                
                // Create key-value pair (store in assignment node for now)
                ASTNode* pair = newNode(NODE_ASSIGN);
                if (pair) {
                    pair->data.name = key;
                    pair->left = value;
                }
                
                if (!first_pair) {
                    first_pair = pair;
                    current_pair = pair;
                } else {
                    current_pair->right = pair;
                    current_pair = pair;
                }
                
            } while (match(TK_COMMA));
            
            if (!match(TK_RBRACE)) {
                error("Expected '}' after object");
            }
            
            node->left = first_pair;
        }
        return node;
    }
    
    // JSON literal with json keyword
    if (match(TK_JSON)) {
        if (!match(TK_STRING)) {
            error("Expected JSON string after 'json'");
            return NULL;
        }
        
        ASTNode* node = newNode(NODE_JSON);
        if (node) {
            node->data.data_literal.data = str_copy(previous.value.str_val);
            node->data.data_literal.format = str_copy("json");
        }
        return node;
    }
    
    error("Expected expression");
    return NULL;
}

// ======================================================
// [SECTION] WELD STATEMENT (INPUT)
// ======================================================
static ASTNode* weldStatement() {
    ASTNode* node = newNode(NODE_WELD);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'weld'");
        free(node);
        return NULL;
    }
    
    // Optional prompt
    if (!match(TK_RPAREN)) {
        node->left = expression();
        if (!match(TK_RPAREN)) {
            error("Expected ')' after weld arguments");
            free(node);
            return NULL;
        }
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after weld statement");
    }
    
    return node;
}

// ======================================================
// [SECTION] PASS STATEMENT
// ======================================================
static ASTNode* passStatement() {
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after pass");
    }
    return newNode(NODE_PASS);
}

// ======================================================
// [SECTION] TYPEDEF DECLARATION
// ======================================================
static ASTNode* typedefDeclaration() {
    ASTNode* node = newNode(NODE_TYPEDEF);
    
    if (!match(TK_IDENT)) {
        error("Expected type name after typedef");
        free(node);
        return NULL;
    }
    
    node->data.name = str_copy(previous.value.str_val);
    
    if (!match(TK_ASSIGN)) {
        error("Expected '=' in typedef");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    // Parse type expression (simplified)
    if (match(TK_TYPE_INT) || match(TK_TYPE_FLOAT) || 
        match(TK_TYPE_STR) || match(TK_TYPE_BOOL)) {
        // Store type name
        printf("%s[TYPEDEF]%s Type '%s' defined\n", 
               COLOR_CYAN, COLOR_RESET, node->data.name);
    } else {
        error("Expected type specification");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after typedef");
    }
    
    return node;
}

// ======================================================
// [SECTION] CLASS DECLARATION
// ======================================================
static ASTNode* classDeclaration() {
    ASTNode* node = newNode(NODE_CLASS);
    
    if (!match(TK_IDENT)) {
        error("Expected class name");
        free(node);
        return NULL;
    }
    
    node->data.class_def.name = str_copy(previous.value.str_val);
    
    // Optional inheritance
    if (match(TK_COLON)) {
        if (!match(TK_IDENT)) {
            error("Expected parent class name");
            free(node->data.class_def.name);
            free(node);
            return NULL;
        }
        node->data.class_def.parent = newIdentNode(previous.value.str_val);
    }
    
    if (!match(TK_LBRACE)) {
        error("Expected '{' before class body");
        free(node->data.class_def.name);
        free(node);
        return NULL;
    }
    
    // Parse class members
    ASTNode* first_member = NULL;
    ASTNode* current_member = NULL;
    
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        ASTNode* member = NULL;
        
        // Parse member (simplified - just variable declarations for now)
        if (match(TK_VAR) || match(TK_LET) || match(TK_CONST)) {
            member = variableDeclaration();
        } else if (match(TK_FUNC)) {
            member = functionDeclaration();
        } else {
            error("Expected class member");
            advance();
            continue;
        }
        
        if (member) {
            if (!first_member) {
                first_member = member;
                current_member = member;
            } else {
                current_member->right = member;
                current_member = member;
            }
        }
    }
    
    if (!match(TK_RBRACE)) {
        error("Expected '}' after class body");
    }
    
    node->data.class_def.members = first_member;
    
    printf("%s[CLASS]%s Class '%s' defined\n", 
           COLOR_MAGENTA, COLOR_RESET, node->data.class_def.name);
    
    return node;
}

// ======================================================
// [SECTION] IMPORT STATEMENT PARSING
// ======================================================
static ASTNode* parseImport() {
    ASTNode* node = newNode(NODE_IMPORT);
    
    if (!match(TK_STRING)) {
        error("Expected module name after import");
        free(node);
        return NULL;
    }
    
    char** imports = malloc(sizeof(char*));
    if (!imports) {
        free(node);
        return NULL;
    }
    
    imports[0] = str_copy(previous.value.str_val);
    int import_count = 1;
    
    // Parse multiple imports separated by commas
    while (match(TK_COMMA)) {
        if (!match(TK_STRING)) {
            error("Expected module name after comma");
            break;
        }
        
        char** new_imports = realloc(imports, (import_count + 1) * sizeof(char*));
        if (!new_imports) {
            for (int i = 0; i < import_count; i++) free(imports[i]);
            free(imports);
            free(node);
            return NULL;
        }
        imports = new_imports;
        imports[import_count++] = str_copy(previous.value.str_val);
    }
    
    // Parse optional 'from' clause
    if (match(TK_FROM)) {
        if (!match(TK_STRING)) {
            error("Expected package name after 'from'");
            for (int i = 0; i < import_count; i++) free(imports[i]);
            free(imports);
            free(node);
            return NULL;
        }
        node->data.imports.from_module = str_copy(previous.value.str_val);
    } else {
        node->data.imports.from_module = NULL;
    }
    
    node->data.imports.modules = imports;
    node->data.imports.module_count = import_count;
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after import statement");
    }
    
    return node;
}

// ======================================================
// [SECTION] EXPORT STATEMENT PARSING
// ======================================================
static ASTNode* parseExport() {
    ASTNode* node = newNode(NODE_EXPORT);
    
    if (!match(TK_STRING)) {
        error("Expected symbol name after export");
        free(node);
        return NULL;
    }
    
    node->data.export.symbol = str_copy(previous.value.str_val);
    
    if (match(TK_AS)) {
        if (!match(TK_STRING)) {
            error("Expected alias name after 'as'");
            free(node->data.export.symbol);
            free(node);
            return NULL;
        }
        node->data.export.alias = str_copy(previous.value.str_val);
    } else {
        node->data.export.alias = str_copy(node->data.export.symbol);
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after export statement");
    }
    
    return node;
}

// ======================================================
// [SECTION] STATEMENT PARSING
// ======================================================
static ASTNode* statement();
static ASTNode* declaration();
static ASTNode* block();
static ASTNode* ifStatement();
static ASTNode* whileStatement();
static ASTNode* forStatement();
static ASTNode* forInStatement();
static ASTNode* returnStatement();
static ASTNode* printStatement();
static ASTNode* variableDeclaration();
static ASTNode* functionDeclaration();
static ASTNode* expressionStatement();

static ASTNode* statement() {
    if (match(TK_PRINT)) return printStatement();
    if (match(TK_WELD)) return weldStatement();
    if (match(TK_PASS)) return passStatement();
    if (match(TK_IF)) return ifStatement();
    if (match(TK_WHILE)) return whileStatement();
    if (match(TK_FOR)) {
        // Check if it's for-in or regular for
        Token saved = current;
        initLexer(current.start); // Reset to check
        
        // Simple heuristic: if next is IDENT and then IN, it's for-in
        advance(); // Skip 'for'
        if (match(TK_IDENT) && match(TK_IN)) {
            // It's for-in
            initLexer(saved.start);
            return forInStatement();
        } else {
            // Regular for
            initLexer(saved.start);
            return forStatement();
        }
    }
    if (match(TK_RETURN)) return returnStatement();
    if (match(TK_LBRACE)) return block();
    
    return expressionStatement();
}

static ASTNode* expressionStatement() {
    ASTNode* expr = expression();
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after expression");
    }
    return expr;
}

static ASTNode* block() {
    ASTNode* node = newNode(NODE_BLOCK);
    ASTNode* current = NULL;
    
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        ASTNode* stmt = declaration();
        if (stmt) {
            if (!node->left) {
                node->left = stmt;
                current = stmt;
            } else {
                current->right = stmt;
                current = stmt;
            }
        }
    }
    
    if (!match(TK_RBRACE)) {
        error("Expected '}' after block");
    }
    
    return node;
}

static ASTNode* ifStatement() {
    ASTNode* node = newNode(NODE_IF);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'if'");
        free(node);
        return NULL;
    }
    
    node->left = expression();
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after if condition");
        free(node);
        return NULL;
    }
    
    node->right = statement();
    
    if (match(TK_ELSE)) {
        node->third = statement();
    }
    
    return node;
}

static ASTNode* whileStatement() {
    ASTNode* node = newNode(NODE_WHILE);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'while'");
        free(node);
        return NULL;
    }
    
    node->left = expression();
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after while condition");
        free(node);
        return NULL;
    }
    
    node->right = statement();
    
    return node;
}

static ASTNode* forStatement() {
    ASTNode* node = newNode(NODE_FOR);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'for'");
        free(node);
        return NULL;
    }
    
    // Initialization
    if (match(TK_SEMICOLON)) {
        // No initialization
    } else if (match(TK_VAR) || match(TK_LET) || match(TK_CONST) ||
               match(TK_NET) || match(TK_CLOG) || match(TK_DOS) || match(TK_SEL)) {
        node->data.loop.init = variableDeclaration();
    } else {
        node->data.loop.init = expressionStatement();
    }
    
    // Condition
    if (!check(TK_SEMICOLON)) {
        node->data.loop.condition = expression();
    }
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after loop condition");
    }
    
    // Increment
    if (!check(TK_RPAREN)) {
        node->data.loop.update = expression();
    }
    if (!match(TK_RPAREN)) {
        error("Expected ')' after for clauses");
    }
    
    node->data.loop.body = statement();
    
    return node;
}

static ASTNode* forInStatement() {
    ASTNode* node = newNode(NODE_FOR_IN);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'for'");
        free(node);
        return NULL;
    }
    
    if (!match(TK_IDENT)) {
        error("Expected variable name in for-in loop");
        free(node);
        return NULL;
    }
    
    node->data.for_in.var_name = str_copy(previous.value.str_val);
    
    if (!match(TK_IN)) {
        error("Expected 'in' in for-in loop");
        free(node->data.for_in.var_name);
        free(node);
        return NULL;
    }
    
    node->data.for_in.iterable = expression();
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after for-in expression");
        free(node->data.for_in.var_name);
        free(node);
        return NULL;
    }
    
    node->data.for_in.body = statement();
    
    return node;
}

static ASTNode* returnStatement() {
    ASTNode* node = newNode(NODE_RETURN);
    
    if (!check(TK_SEMICOLON)) {
        node->left = expression();
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after return value");
    }
    
    return node;
}

static ASTNode* printStatement() {
    ASTNode* node = newNode(NODE_PRINT);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'print'");
        free(node);
        return NULL;
    }
    
    node->left = expression();
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after print arguments");
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after print statement");
        free(node);
        return NULL;
    }
    
    return node;
}

// ======================================================
// [SECTION] VARIABLE DECLARATION
// ======================================================
static ASTNode* variableDeclaration() {
    TokenKind declType = previous.kind;
    
    if (!match(TK_IDENT)) {
        error("Expected variable name");
        return NULL;
    }
    
    char* varName = str_copy(previous.value.str_val);
    ASTNode* node = NULL;
    
    switch (declType) {
        case TK_VAR: node = newNode(NODE_VAR_DECL); break;
        case TK_NET: node = newNode(NODE_NET_DECL); break;
        case TK_CLOG: node = newNode(NODE_CLOG_DECL); break;
        case TK_DOS: node = newNode(NODE_DOS_DECL); break;
        case TK_SEL: node = newNode(NODE_SEL_DECL); break;
        case TK_LET: 
        case TK_CONST: node = newNode(NODE_CONST_DECL); break;
        default: node = newNode(NODE_VAR_DECL); break;
    }
    
    if (!node) {
        free(varName);
        return NULL;
    }
    
    node->data.name = varName;
    
    if (match(TK_ASSIGN)) {
        node->left = expression();
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after variable declaration");
    }
    
    return node;
}

// ======================================================
// [SECTION] FUNCTION DECLARATION
// ======================================================
static ASTNode* functionDeclaration() {
    if (!match(TK_IDENT)) {
        error("Expected function name");
        return NULL;
    }
    
    char* func_name = str_copy(previous.value.str_val);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after function name");
        free(func_name);
        return NULL;
    }
    
    // Parse parameters
    char** params = NULL;
    int param_count = 0;
    
    if (!match(TK_RPAREN)) {
        if (match(TK_IDENT)) {
            params = malloc(sizeof(char*));
            if (params) {
                params[0] = str_copy(previous.value.str_val);
                param_count = 1;
            }
            
            while (match(TK_COMMA)) {
                if (!match(TK_IDENT)) {
                    error("Expected parameter name");
                    break;
                }
                
                char** new_params = realloc(params, (param_count + 1) * sizeof(char*));
                if (!new_params) {
                    for (int i = 0; i < param_count; i++) free(params[i]);
                    free(params);
                    free(func_name);
                    return NULL;
                }
                params = new_params;
                params[param_count++] = str_copy(previous.value.str_val);
            }
            
            if (!match(TK_RPAREN)) {
                error("Expected ')' after parameters");
            }
        }
    }
    
    if (!match(TK_LBRACE)) {
        error("Expected '{' before function body");
        if (params) {
            for (int i = 0; i < param_count; i++) free(params[i]);
            free(params);
        }
        free(func_name);
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_FUNC);
    if (!node) {
        if (params) {
            for (int i = 0; i < param_count; i++) free(params[i]);
            free(params);
        }
        free(func_name);
        return NULL;
    }
    
    node->data.name = func_name;
    
    // Store parameters
    if (param_count > 0) {
        ASTNode* param_list = NULL;
        ASTNode* current_param = NULL;
        
        for (int i = 0; i < param_count; i++) {
            ASTNode* param_node = newIdentNode(params[i]);
            if (param_node) {
                if (!param_list) {
                    param_list = param_node;
                    current_param = param_node;
                } else {
                    current_param->right = param_node;
                    current_param = param_node;
                }
            }
            free(params[i]);
        }
        free(params);
        
        node->left = param_list;
    }
    
    // Parse function body
    scope_level++;
    node->right = block();
    scope_level--;
    
    printf("%s[FUNC]%s Function '%s' declared with %d parameters\n", 
           COLOR_GREEN, COLOR_RESET, func_name, param_count);
    
    return node;
}

// ======================================================
// [SECTION] MAIN DECLARATION (IMPROVED)
// ======================================================
static ASTNode* mainDeclaration() {
    ASTNode* node = newNode(NODE_MAIN);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after main");
        free(node);
        return NULL;
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after '('");
        free(node);
        return NULL;
    }
    
    if (!match(TK_LBRACE)) {
        error("Expected '{' before main body");
        free(node);
        return NULL;
    }
    
    node->left = block();
    
    printf("%s[MAIN]%s Main function parsed\n", COLOR_BLUE, COLOR_RESET);
    
    return node;
}

// ======================================================
// [SECTION] DBVAR COMMAND
// ======================================================
static ASTNode* dbvarCommand() {
    ASTNode* node = newNode(NODE_DBVAR);
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after dbvar");
    }
    
    return node;
}

// ======================================================
// [SECTION] DECLARATION PARSING (COMPLETE)
// ======================================================
static ASTNode* declaration() {
    if (match(TK_MAIN)) return mainDeclaration();
    if (match(TK_DBVAR)) return dbvarCommand();
    if (match(TK_IMPORT)) return parseImport();
    if (match(TK_EXPORT)) return parseExport();
    if (match(TK_CLASS)) return classDeclaration();
    if (match(TK_TYPEDEF)) return typedefDeclaration();
    
    if (match(TK_FUNC)) return functionDeclaration();
    
    if (match(TK_VAR) || match(TK_LET) || match(TK_CONST) ||
        match(TK_NET) || match(TK_CLOG) || match(TK_DOS) || match(TK_SEL)) {
        return variableDeclaration();
    }
    
    return statement();
}

// ======================================================
// [SECTION] MAIN PARSER FUNCTION
// ======================================================
ASTNode** parse(const char* source, int* count) {
    initLexer(source);
    advance();
    hadError = false;
    panicMode = false;
    errorCount = 0;
    scope_level = 0;
    
    printf("%s[PARSER]%s Starting parse...\n", COLOR_CYAN, COLOR_RESET);
    
    int capacity = 50;
    ASTNode** nodes = malloc(capacity * sizeof(ASTNode*));
    *count = 0;
    
    if (!nodes) {
        printf("%s[PARSER]%s Memory allocation failed\n", COLOR_RED, COLOR_RESET);
        return NULL;
    }
    
    while (current.kind != TK_EOF) {
        if (*count >= capacity) {
            capacity *= 2;
            ASTNode** new_nodes = realloc(nodes, capacity * sizeof(ASTNode*));
            if (!new_nodes) {
                printf("%s[PARSER]%s Memory reallocation failed\n", COLOR_RED, COLOR_RESET);
                break;
            }
            nodes = new_nodes;
        }
        
        ASTNode* node = declaration();
        if (node) {
            nodes[(*count)++] = node;
        } else {
            printf("%s[PARSER]%s Failed to parse declaration\n", COLOR_YELLOW, COLOR_RESET);
        }
        
        if (panicMode) {
            printf("%s[PARSER]%s Panic mode - synchronizing\n", COLOR_YELLOW, COLOR_RESET);
            synchronize();
        }
    }
    
    printf("%s[PARSER]%s Parse complete. %d nodes parsed.\n", 
           COLOR_CYAN, COLOR_RESET, *count);
    
    if (hadError) {
        printf("%s[PARSER]%s Parse completed with errors\n", COLOR_RED, COLOR_RESET);
    } else {
        printf("%s[PARSER]%s Parse successful\n", COLOR_GREEN, COLOR_RESET);
    }
    
    return nodes;
}

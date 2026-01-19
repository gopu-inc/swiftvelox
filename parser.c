#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>
#include "common.h"

extern Token scanToken();
extern void initLexer(const char* source, const char* filename);
extern bool isAtEnd();

// ======================================================
// [SECTION] PARSER STATE
// ======================================================
static Token current;
static Token previous;
static bool hadError = false;
static bool panicMode = false;
static const char* current_filename;

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

static bool checkNext(TokenKind kind) {
    if (isAtEnd()) return false;
    Token saved = current;
    advance();
    bool result = (current.kind == kind);
    current = saved;
    return result;
}

static void errorAt(Token token, const char* message) {
    if (panicMode) return;
    panicMode = true;
    
    fprintf(stderr, RED "[PARSER ERROR]" RESET " %s:%d:%d: %s\n", 
            current_filename, token.line, token.column, message);
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
            case TK_PROC:
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
            case TK_RETURN:
            case TK_IMPORT:
            case TK_EXPORT:
            case TK_MAIN:
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
        node->is_public = false;
        node->is_private = false;
        node->is_protected = false;
        node->doc_comment = NULL;
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

static ASTNode* newNullNode() {
    return newNode(NODE_NULL);
}

static ASTNode* newUndefinedNode() {
    return newNode(NODE_UNDEFINED);
}

// ======================================================
// [SECTION] EXPRESSION PARSING (RECURSIVE DESCENT)
// ======================================================
static ASTNode* expression();
static ASTNode* assignment();
static ASTNode* ternary();
static ASTNode* logicalOr();
static ASTNode* logicalAnd();
static ASTNode* bitwiseOr();
static ASTNode* bitwiseXor();
static ASTNode* bitwiseAnd();
static ASTNode* equality();
static ASTNode* comparison();
static ASTNode* shift();
static ASTNode* term();
static ASTNode* factor();
static ASTNode* unary();
static ASTNode* call();
static ASTNode* primary();

static ASTNode* expression() {
    return assignment();
}

static ASTNode* assignment() {
    ASTNode* expr = ternary();
    
    if (match(TK_ASSIGN) || match(TK_PLUS_ASSIGN) || 
        match(TK_MINUS_ASSIGN) || match(TK_MULT_ASSIGN) ||
        match(TK_DIV_ASSIGN) || match(TK_MOD_ASSIGN) ||
        match(TK_POW_ASSIGN) || match(TK_AND_ASSIGN) ||
        match(TK_OR_ASSIGN) || match(TK_XOR_ASSIGN) ||
        match(TK_SHL_ASSIGN) || match(TK_SHR_ASSIGN)) {
        
        TokenKind op = previous.kind;
        ASTNode* value = assignment();
        
        if (expr->type != NODE_IDENT) {
            error("Invalid assignment target");
            return expr;
        }
        
        ASTNode* node = newNode(op == TK_ASSIGN ? NODE_ASSIGN : NODE_COMPOUND_ASSIGN);
        if (node) {
            node->data.name = str_copy(expr->data.name);
            node->left = value;
            node->op_type = op;
        }
        
        // Free old ident node
        free(expr->data.name);
        free(expr);
        
        return node;
    }
    
    return expr;
}

static ASTNode* ternary() {
    ASTNode* expr = logicalOr();
    
    if (match(TK_QUESTION)) {
        ASTNode* node = newNode(NODE_TERNARY);
        if (node) {
            node->left = expr; // Condition
            node->right = expression(); // Then branch
            
            if (!match(TK_COLON)) {
                error("Expected ':' in ternary operator");
                return node;
            }
            
            node->third = expression(); // Else branch
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* logicalOr() {
    ASTNode* expr = logicalAnd();
    
    while (match(TK_OR)) {
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = TK_OR;
            node->left = expr;
            node->right = logicalAnd();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* logicalAnd() {
    ASTNode* expr = bitwiseOr();
    
    while (match(TK_AND)) {
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = TK_AND;
            node->left = expr;
            node->right = bitwiseOr();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* bitwiseOr() {
    ASTNode* expr = bitwiseXor();
    
    while (match(TK_BIT_OR)) {
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = TK_BIT_OR;
            node->left = expr;
            node->right = bitwiseXor();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* bitwiseXor() {
    ASTNode* expr = bitwiseAnd();
    
    while (match(TK_BIT_XOR)) {
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = TK_BIT_XOR;
            node->left = expr;
            node->right = bitwiseAnd();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* bitwiseAnd() {
    ASTNode* expr = equality();
    
    while (match(TK_BIT_AND)) {
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = TK_BIT_AND;
            node->left = expr;
            node->right = equality();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* equality() {
    ASTNode* expr = comparison();
    
    while (match(TK_EQ) || match(TK_NEQ)) {
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
    ASTNode* expr = shift();
    
    while (match(TK_GT) || match(TK_GTE) || match(TK_LT) || match(TK_LTE)) {
        TokenKind op = previous.kind;
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = op;
            node->left = expr;
            node->right = shift();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* shift() {
    ASTNode* expr = term();
    
    while (match(TK_SHL) || match(TK_SHR)) {
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
    
    while (match(TK_PLUS) || match(TK_MINUS)) {
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
    if (match(TK_MINUS) || match(TK_NOT) || match(TK_BIT_NOT) || 
        match(TK_INC) || match(TK_DEC) || match(TK_PLUS)) {
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
    ASTNode* expr = primary();
    
    while (true) {
        if (match(TK_LPAREN)) {
            // Function call
            ASTNode* node = newNode(NODE_FUNC_CALL);
            if (node) {
                node->left = expr; // Function name
                node->right = NULL; // Arguments list (to be built)
                
                // Parse arguments
                if (!check(TK_RPAREN)) {
                    ASTNode* args = expression();
                    ASTNode* current_arg = args;
                    
                    while (match(TK_COMMA)) {
                        ASTNode* next_arg = expression();
                        current_arg->right = next_arg;
                        current_arg = next_arg;
                    }
                    
                    node->right = args;
                }
                
                if (!match(TK_RPAREN)) {
                    error("Expected ')' after arguments");
                }
                
                expr = node;
            }
        } else if (match(TK_PERIOD) || match(TK_SAFE_NAV)) {
            // Member access
            if (!match(TK_IDENT)) {
                error("Expected property name after '.'");
                return expr;
            }
            
            ASTNode* node = newNode(NODE_BINARY);
            if (node) {
                node->op_type = previous.kind == TK_PERIOD ? TK_PERIOD : TK_SAFE_NAV;
                node->left = expr;
                node->right = newIdentNode(previous.value.str_val);
                expr = node;
            }
        } else if (match(TK_LBRACKET)) {
            // Array/map access
            ASTNode* node = newNode(NODE_BINARY);
            if (node) {
                node->op_type = TK_LBRACKET;
                node->left = expr;
                node->right = expression();
                
                if (!match(TK_RBRACKET)) {
                    error("Expected ']' after index");
                }
                
                expr = node;
            }
        } else {
            break;
        }
    }
    
    return expr;
}

static ASTNode* primary() {
    if (match(TK_TRUE)) return newBoolNode(true);
    if (match(TK_FALSE)) return newBoolNode(false);
    if (match(TK_NULL)) return newNullNode();
    if (match(TK_UNDEFINED)) return newUndefinedNode();
    if (match(TK_NAN)) return newNode(NODE_NAN);
    if (match(TK_INF)) {
        ASTNode* node = newNode(NODE_FLOAT);
        if (node) node->data.float_val = INFINITY;
        return node;
    }
    
    if (match(TK_INT)) return newIntNode(previous.value.int_val);
    if (match(TK_FLOAT)) return newFloatNode(previous.value.float_val);
    if (match(TK_STRING)) return newStringNode(previous.value.str_val);
    
    if (match(TK_IDENT)) return newIdentNode(previous.value.str_val);
    
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
        // Array literal
        ASTNode* node = newNode(NODE_ARRAY);
        if (node) {
            node->data.collection.elements = NULL;
            node->data.collection.element_count = 0;
            
            if (!check(TK_RBRACKET)) {
                int capacity = 10;
                node->data.collection.elements = malloc(capacity * sizeof(ASTNode*));
                
                do {
                    if (node->data.collection.element_count >= capacity) {
                        capacity *= 2;
                        node->data.collection.elements = realloc(
                            node->data.collection.elements, 
                            capacity * sizeof(ASTNode*)
                        );
                    }
                    
                    node->data.collection.elements[node->data.collection.element_count++] = expression();
                } while (match(TK_COMMA) && !check(TK_RBRACKET));
            }
            
            if (!match(TK_RBRACKET)) {
                error("Expected ']' after array literal");
            }
        }
        return node;
    }
    
    if (match(TK_LBRACE)) {
        // Map literal or block
        if (check(TK_IDENT) && checkNext(TK_COLON)) {
            // Map literal
            ASTNode* node = newNode(NODE_MAP);
            if (node) {
                node->data.collection.elements = NULL;
                node->data.collection.element_count = 0;
                
                if (!check(TK_RBRACE)) {
                    int capacity = 10;
                    node->data.collection.elements = malloc(capacity * sizeof(ASTNode*));
                    
                    do {
                        if (node->data.collection.element_count >= capacity) {
                            capacity *= 2;
                            node->data.collection.elements = realloc(
                                node->data.collection.elements, 
                                capacity * sizeof(ASTNode*)
                            );
                        }
                        
                        if (!match(TK_IDENT)) {
                            error("Expected identifier as map key");
                            break;
                        }
                        char* key = str_copy(previous.value.str_val);
                        
                        if (!match(TK_COLON)) {
                            error("Expected ':' after map key");
                            free(key);
                            break;
                        }
                        
                        ASTNode* pair = newNode(NODE_BINARY);
                        if (pair) {
                            pair->op_type = TK_COLON;
                            pair->left = newIdentNode(key);
                            pair->right = expression();
                            free(key);
                            
                            node->data.collection.elements[node->data.collection.element_count++] = pair;
                        }
                    } while (match(TK_COMMA) && !check(TK_RBRACE));
                }
                
                if (!match(TK_RBRACE)) {
                    error("Expected '}' after map literal");
                }
            }
            return node;
        } else {
            // Block
            ASTNode* node = newNode(NODE_BLOCK);
            
            while (!check(TK_RBRACE) && !check(TK_EOF)) {
                ASTNode* stmt = expression();
                if (stmt) {
                    // Add to block
                    if (!node->left) {
                        node->left = stmt;
                    } else {
                        // Chain statements
                        ASTNode* current = node->left;
                        while (current->right) {
                            current = current->right;
                        }
                        current->right = stmt;
                    }
                }
                
                if (!match(TK_SEMICOLON) && !check(TK_RBRACE)) {
                    error("Expected ';' after statement");
                }
            }
            
            if (!match(TK_RBRACE)) {
                error("Expected '}' after block");
            }
            
            return node;
        }
    }
    
    errorAtCurrent("Expected expression");
    return NULL;
}

// ======================================================
// [SECTION] IMPORT STATEMENT PARSING (IMPROVED)
// ======================================================
static ASTNode* parseImport() {
    ASTNode* node = newNode(NODE_IMPORT);
    
    // Syntax options:
    // 1. import "module"
    // 2. import "module" as alias
    // 3. import "module1", "module2" from "package"
    // 4. import * from "package"
    // 5. import {symbol1, symbol2} from "module"
    
    // Check for named imports {symbol1, symbol2}
    if (match(TK_LBRACE)) {
        node->data.imports.is_wildcard = false;
        
        char** imports = malloc(sizeof(char*));
        if (!imports) {
            free(node);
            return NULL;
        }
        
        imports[0] = NULL;
        int import_count = 0;
        
        // Parse named imports
        do {
            if (!match(TK_IDENT)) {
                error("Expected identifier in named import");
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
            
            // Optional alias
            if (match(TK_AS)) {
                if (!match(TK_IDENT)) {
                    error("Expected alias name after 'as'");
                    break;
                }
                // For simplicity, we'll just use the original name
                // In a full implementation, we'd store both
            }
        } while (match(TK_COMMA) && !check(TK_RBRACE));
        
        if (!match(TK_RBRACE)) {
            error("Expected '}' after named imports");
        }
        
        node->data.imports.modules = imports;
        node->data.imports.module_count = import_count;
    } 
    // Check for wildcard import
    else if (match(TK_MULT)) {
        node->data.imports.is_wildcard = true;
        node->data.imports.modules = malloc(sizeof(char*));
        node->data.imports.modules[0] = str_copy("*");
        node->data.imports.module_count = 1;
    }
    // Regular string import
    else if (match(TK_STRING)) {
        node->data.imports.is_wildcard = false;
        node->data.imports.modules = malloc(sizeof(char*));
        node->data.imports.modules[0] = str_copy(previous.value.str_val);
        node->data.imports.module_count = 1;
        
        // Optional alias
        if (match(TK_AS)) {
            if (!match(TK_IDENT)) {
                error("Expected alias name after 'as'");
            } else {
                node->data.imports.alias = str_copy(previous.value.str_val);
            }
        }
    } else {
        error("Expected module name or wildcard after import");
        free(node);
        return NULL;
    }
    
    // Check for 'from' clause
    if (match(TK_FROM)) {
        if (!match(TK_STRING)) {
            error("Expected package name after 'from'");
            for (int i = 0; i < node->data.imports.module_count; i++) {
                free(node->data.imports.modules[i]);
            }
            free(node->data.imports.modules);
            free(node);
            return NULL;
        }
        node->data.imports.from_module = str_copy(previous.value.str_val);
    } else {
        node->data.imports.from_module = NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after import statement");
    }
    
    return node;
}

// ======================================================
// [SECTION] EXPORT STATEMENT PARSING (IMPROVED)
// ======================================================
static ASTNode* parseExport() {
    ASTNode* node = newNode(NODE_EXPORT);
    
    // Syntax options:
    // 1. export var_name;
    // 2. export var_name as alias;
    // 3. export default expression;
    // 4. export {name1, name2};
    // 5. export function func() { ... }
    
    // Check for default export
    if (match(TK_DEFAULT)) {
        node->data.export.is_default = true;
        
        // Could be a variable, function, or expression
        if (match(TK_IDENT)) {
            node->data.export.symbol = str_copy(previous.value.str_val);
            node->data.export.alias = str_copy("default");
        } else {
            // For now, just support identifier exports
            error("Expected identifier after 'export default'");
            free(node);
            return NULL;
        }
    }
    // Check for named exports {name1, name2}
    else if (match(TK_LBRACE)) {
        // Simplified - just parse first identifier
        if (!match(TK_IDENT)) {
            error("Expected identifier in export list");
            free(node);
            return NULL;
        }
        
        node->data.export.symbol = str_copy(previous.value.str_val);
        node->data.export.is_default = false;
        
        // Optional alias
        if (match(TK_AS)) {
            if (!match(TK_IDENT)) {
                error("Expected alias name after 'as'");
                free(node->data.export.symbol);
                free(node);
                return NULL;
            }
            node->data.export.alias = str_copy(previous.value.str_val);
        } else {
            node->data.export.alias = str_copy(node->data.export.symbol);
        }
        
        // Skip rest of list for now
        while (match(TK_COMMA) && !check(TK_RBRACE)) {
            if (!match(TK_IDENT)) break;
        }
        
        if (!match(TK_RBRACE)) {
            error("Expected '}' after export list");
        }
    }
    // Regular export
    else {
        node->data.export.is_default = false;
        
        if (!match(TK_IDENT)) {
            error("Expected identifier after export");
            free(node);
            return NULL;
        }
        
        node->data.export.symbol = str_copy(previous.value.str_val);
        
        // Optional alias
        if (match(TK_AS)) {
            if (!match(TK_IDENT)) {
                error("Expected alias name after 'as'");
                free(node->data.export.symbol);
                free(node);
                return NULL;
            }
            node->data.export.alias = str_copy(previous.value.str_val);
        } else {
            node->data.export.alias = str_copy(node->data.export.symbol);
        }
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after export statement");
    }
    
    return node;
}

// ======================================================
// [SECTION] VARIABLE DECLARATION PARSING
// ======================================================
static ASTNode* variableDeclaration() {
    TokenKind declType = previous.kind;
    
    if (!match(TK_IDENT)) {
        error("Expected variable name");
        return NULL;
    }
    
    char* varName = str_copy(previous.value.str_val);
    ASTNode* node = NULL;
    
    // Determine node type based on declaration keyword
    switch (declType) {
        case TK_VAR: node = newNode(NODE_VAR_DECL); break;
        case TK_NET: node = newNode(NODE_NET_DECL); break;
        case TK_CLOG: node = newNode(NODE_CLOG_DECL); break;
        case TK_DOS: node = newNode(NODE_DOS_DECL); break;
        case TK_SEL: node = newNode(NODE_SEL_DECL); break;
        case TK_LET: 
        case TK_CONST: node = newNode(NODE_CONST_DECL); break;
        case TK_STATIC: node = newNode(NODE_STATIC_DECL); break;
        case TK_REF: node = newNode(NODE_REF_DECL); break;
        default: node = newNode(NODE_VAR_DECL); break;
    }
    
    if (!node) {
        free(varName);
        return NULL;
    }
    
    node->data.name = varName;
    
    // Optional type annotation
    if (match(TK_COLON)) {
        // Type annotation - for now just skip
        if (!match(TK_IDENT)) {
            error("Expected type name after ':'");
        }
    }
    
    // Optional initialization
    if (match(TK_ASSIGN) || match(TK_DARROW)) {
        node->left = expression();
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after variable declaration");
    }
    
    return node;
}

// ======================================================
// [SECTION] FUNCTION DECLARATION PARSING
// ======================================================
static ASTNode* functionDeclaration() {
    ASTNode* node = newNode(NODE_FUNC_DECL);
    
    if (!match(TK_IDENT)) {
        error("Expected function name");
        free(node);
        return NULL;
    }
    
    node->data.func.func_name = str_copy(previous.value.str_val);
    node->data.func.params = NULL;
    node->data.func.body = NULL;
    node->data.func.return_type = NULL;
    node->data.func.is_async = false;
    node->data.func.is_extern = false;
    
    // Check for async
    if (strcmp(node->data.func.func_name, "async") == 0) {
        node->data.func.is_async = true;
        free(node->data.func.func_name);
        
        if (!match(TK_IDENT)) {
            error("Expected function name after 'async'");
            free(node);
            return NULL;
        }
        node->data.func.func_name = str_copy(previous.value.str_val);
    }
    
    // Check for extern
    if (strcmp(node->data.func.func_name, "extern") == 0) {
        node->data.func.is_extern = true;
        free(node->data.func.func_name);
        
        if (!match(TK_IDENT)) {
            error("Expected function name after 'extern'");
            free(node);
            return NULL;
        }
        node->data.func.func_name = str_copy(previous.value.str_val);
    }
    
    // Parse parameters
    if (!match(TK_LPAREN)) {
        error("Expected '(' after function name");
        free(node->data.func.func_name);
        free(node);
        return NULL;
    }
    
    ASTNode* params = newNode(NODE_PARAM_LIST);
    if (!check(TK_RPAREN)) {
        // Parse parameter list
        ASTNode* current_param = NULL;
        
        do {
            if (!match(TK_IDENT)) {
                error("Expected parameter name");
                break;
            }
            
            ASTNode* param = newIdentNode(previous.value.str_val);
            
            // Optional type annotation
            if (match(TK_COLON)) {
                if (!match(TK_IDENT)) {
                    error("Expected type name after ':'");
                }
                // Type info would be stored here
            }
            
            if (!params->left) {
                params->left = param;
                current_param = param;
            } else {
                current_param->right = param;
                current_param = param;
            }
        } while (match(TK_COMMA) && !check(TK_RPAREN));
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after parameters");
    }
    
    node->data.func.params = params;
    
    // Optional return type
    if (match(TK_RARROW)) {
        if (!match(TK_IDENT)) {
            error("Expected return type after '->'");
        } else {
            node->data.func.return_type = newIdentNode(previous.value.str_val);
        }
    }
    
    // Function body
    if (node->data.func.is_extern) {
        // Extern function - no body
        if (!match(TK_SEMICOLON)) {
            error("Expected ';' after extern function declaration");
        }
    } else {
        if (!match(TK_LBRACE)) {
            error("Expected '{' before function body");
        } else {
            node->data.func.body = newNode(NODE_BLOCK);
            
            while (!check(TK_RBRACE) && !check(TK_EOF)) {
                ASTNode* stmt = expression();
                if (stmt) {
                    // Add to block
                    if (!node->data.func.body->left) {
                        node->data.func.body->left = stmt;
                    } else {
                        ASTNode* current = node->data.func.body->left;
                        while (current->right) {
                            current = current->right;
                        }
                        current->right = stmt;
                    }
                }
                
                if (!match(TK_SEMICOLON) && !check(TK_RBRACE)) {
                    error("Expected ';' after statement");
                }
            }
            
            if (!match(TK_RBRACE)) {
                error("Expected '}' after function body");
            }
        }
    }
    
    return node;
}

// ======================================================
// [SECTION] CONTROL FLOW STATEMENT PARSING
// ======================================================
static ASTNode* ifStatement() {
    ASTNode* node = newNode(NODE_IF);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'if'");
        free(node);
        return NULL;
    }
    
    node->left = expression(); // Condition
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after if condition");
        free(node);
        return NULL;
    }
    
    // Optional 'then' keyword
    match(TK_THEN);
    
    node->right = expression(); // Then branch
    
    if (match(TK_ELSE)) {
        node->third = expression(); // Else branch
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
    
    node->left = expression(); // Condition
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after while condition");
        free(node);
        return NULL;
    }
    
    node->right = expression(); // Body
    
    return node;
}

static ASTNode* forStatement() {
    ASTNode* node = newNode(NODE_FOR);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'for'");
        free(node);
        return NULL;
    }
    
    // Initializer
    if (match(TK_SEMICOLON)) {
        // No initializer
    } else if (match(TK_VAR) || match(TK_LET) || match(TK_CONST) ||
               match(TK_NET) || match(TK_CLOG) || match(TK_DOS) || match(TK_SEL)) {
        node->data.loop.init = variableDeclaration();
    } else {
        node->data.loop.init = expression();
        if (!match(TK_SEMICOLON)) {
            error("Expected ';' after for loop initializer");
        }
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
    
    node->data.loop.body = expression(); // Body
    
    return node;
}

// ======================================================
// [SECTION] PRINT STATEMENT PARSING
// ======================================================
static ASTNode* printStatement() {
    ASTNode* node = newNode(NODE_PRINT);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'print'");
        free(node);
        return NULL;
    }
    
    node->left = expression();
    
    // Multiple arguments
    while (match(TK_COMMA)) {
        ASTNode* arg = expression();
        if (arg) {
            if (!node->right) {
                node->right = arg;
            } else {
                ASTNode* current = node->right;
                while (current->right) {
                    current = current->right;
                }
                current->right = arg;
            }
        }
    }
    
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
// [SECTION] RETURN STATEMENT PARSING
// ======================================================
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

// ======================================================
// [SECTION] DBVAR COMMAND PARSING
// ======================================================
static ASTNode* dbvarCommand() {
    ASTNode* node = newNode(NODE_DBVAR);
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after dbvar");
    }
    
    return node;
}

// ======================================================
// [SECTION] MAIN DECLARATION PARSING
// ======================================================
static ASTNode* mainDeclaration() {
    if (!match(TK_LPAREN)) {
        error("Expected '(' after main");
        return NULL;
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after '('");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_MAIN);
    
    // Main body
    if (!match(TK_LBRACE)) {
        error("Expected '{' before main body");
        free(node);
        return NULL;
    }
    
    node->left = newNode(NODE_BLOCK);
    
    // Parse block contents
    ASTNode* block = node->left;
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        ASTNode* stmt = expression();
        if (stmt) {
            if (!block->left) {
                block->left = stmt;
            } else {
                ASTNode* current = block->left;
                while (current->right) {
                    current = current->right;
                }
                current->right = stmt;
            }
        }
        
        if (!match(TK_SEMICOLON) && !check(TK_RBRACE)) {
            error("Expected ';' after statement");
        }
    }
    
    if (!match(TK_RBRACE)) {
        error("Expected '}' after main body");
    }
    
    return node;
}

// ======================================================
// [SECTION] DECLARATION PARSING
// ======================================================
static ASTNode* declaration() {
    if (match(TK_MAIN)) return mainDeclaration();
    if (match(TK_DBVAR)) return dbvarCommand();
    if (match(TK_IMPORT)) return parseImport();
    if (match(TK_EXPORT)) return parseExport();
    if (match(TK_FUNC) || match(TK_PROC)) return functionDeclaration();
    
    if (match(TK_VAR) || match(TK_LET) || match(TK_CONST) ||
        match(TK_NET) || match(TK_CLOG) || match(TK_DOS) || match(TK_SEL) ||
        match(TK_STATIC) || match(TK_REF)) {
        return variableDeclaration();
    }
    
    return expression();
}

// ======================================================
// [SECTION] MAIN PARSER FUNCTION
// ======================================================
ASTNode** parse(const char* source, const char* filename, int* count) {
    initLexer(source, filename);
    current_filename = filename;
    advance();
    hadError = false;
    panicMode = false;
    
    int capacity = 10;
    ASTNode** nodes = malloc(capacity * sizeof(ASTNode*));
    *count = 0;
    
    if (!nodes) {
        log_error(filename, 0, 0, "Memory allocation failed");
        return NULL;
    }
    
    while (current.kind != TK_EOF) {
        if (*count >= capacity) {
            capacity *= 2;
            ASTNode** new_nodes = realloc(nodes, capacity * sizeof(ASTNode*));
            if (!new_nodes) {
                log_error(filename, current.line, current.column, 
                         "Memory reallocation failed");
                break;
            }
            nodes = new_nodes;
        }
        
        ASTNode* node = declaration();
        if (node) {
            nodes[(*count)++] = node;
        }
        
        if (panicMode) {
            synchronize();
        }
    }
    
    if (hadError) {
        // Clean up on error
        for (int i = 0; i < *count; i++) {
            if (nodes[i]) free(nodes[i]);
        }
        free(nodes);
        return NULL;
    }
    
    return nodes;
}

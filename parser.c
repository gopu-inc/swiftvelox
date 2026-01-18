#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"

extern Token scanToken();
extern void initLexer(const char* source);

// ======================================================
// [SECTION] PARSER STATE
// ======================================================
static Token current;
static Token previous;
static bool hadError = false;

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

static void errorAtCurrent(const char* message) {
    printf(RED "[ERROR]" RESET " Line %d: %s\n", current.line, message);
    hadError = true;
}

static ASTNode* newNode(NodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    if (node) {
        node->type = type;
        node->import_count = 0;
        node->from_module = NULL;
    }
    return node;
}

// ======================================================
// [SECTION] IMPORT PARSING
// ======================================================
static ASTNode* parseImportStatement() {
    if (!match(TK_LPAREN)) {
        errorAtCurrent("Expected '(' after import");
        return NULL;
    }
    
    if (!match(TK_RPAREN)) {
        errorAtCurrent("Expected ')' after '('");
        return NULL;
    }
    
    if (!match(TK_LBRACE)) {
        errorAtCurrent("Expected '{' for import list");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_IMPORT);
    if (!node) return NULL;
    
    node->from_module = NULL;
    
    char** imports = NULL;
    int count = 0;
    int capacity = 4;
    
    imports = malloc(capacity * sizeof(char*));
    if (!imports) {
        free(node);
        return NULL;
    }
    
    if (match(TK_STRING)) {
        imports[count++] = str_copy(previous.value.str_val);
    } else {
        errorAtCurrent("Expected string in import list");
        free(imports);
        free(node);
        return NULL;
    }
    
    while (match(TK_COMMA)) {
        if (current.kind == TK_IDENT && 
            current.value.str_val && 
            strcmp(current.value.str_val, "from") == 0) {
            
            advance();
            
            if (!match(TK_STRING)) {
                errorAtCurrent("Expected module name after 'from'");
                break;
            }
            
            node->from_module = str_copy(previous.value.str_val);
            break;
        }
        
        if (count >= capacity) {
            capacity *= 2;
            char** new_imports = realloc(imports, capacity * sizeof(char*));
            if (!new_imports) {
                for (int i = 0; i < count; i++) free(imports[i]);
                free(imports);
                free(node);
                return NULL;
            }
            imports = new_imports;
        }
        
        if (match(TK_STRING)) {
            imports[count++] = str_copy(previous.value.str_val);
        } else {
            errorAtCurrent("Expected string after comma");
            break;
        }
    }
    
    if (!match(TK_RBRACE)) {
        errorAtCurrent("Expected '}' after import list");
        for (int i = 0; i < count; i++) free(imports[i]);
        free(imports);
        free(node->from_module);
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        errorAtCurrent("';' expected after import statement");
        for (int i = 0; i < count; i++) free(imports[i]);
        free(imports);
        free(node->from_module);
        free(node);
        return NULL;
    }
    
    node->data.imports = imports;
    node->import_count = count;
    return node;
}

// ======================================================
// [SECTION] EXPRESSION PARSING
// ======================================================
static ASTNode* parseExpression();
static ASTNode* parseAssignment();
static ASTNode* parseEquality();
static ASTNode* parseComparison();
static ASTNode* parseTerm();
static ASTNode* parseFactor();
static ASTNode* parseUnary();
static ASTNode* parsePrimary();
static ASTNode* parseSizeOf();
static ASTNode* parseZis();

static ASTNode* parsePrimary() {
    if (match(TK_INT)) {
        ASTNode* node = newNode(NODE_INT);
        if (node) node->data.int_val = previous.value.int_val;
        return node;
    }
    
    if (match(TK_FLOAT)) {
        ASTNode* node = newNode(NODE_FLOAT);
        if (node) node->data.float_val = previous.value.float_val;
        return node;
    }
    
    if (match(TK_STRING)) {
        ASTNode* node = newNode(NODE_STRING);
        if (node) node->data.str_val = str_copy(previous.value.str_val);
        return node;
    }
    
    if (match(TK_CHAR)) {
        ASTNode* node = newNode(NODE_CHAR);
        if (node) node->data.char_val = previous.value.char_val;
        return node;
    }
    
    if (match(TK_TRUE)) {
        ASTNode* node = newNode(NODE_BOOL);
        if (node) node->data.bool_val = true;
        return node;
    }
    
    if (match(TK_FALSE)) {
        ASTNode* node = newNode(NODE_BOOL);
        if (node) node->data.bool_val = false;
        return node;
    }
    
    if (match(TK_NULL)) {
        ASTNode* node = newNode(NODE_NULL);
        return node;
    }
    
    if (match(TK_IDENT)) {
        ASTNode* node = newNode(NODE_IDENT);
        if (node) node->data.name = str_copy(previous.value.str_val);
        return node;
    }
    
    if (match(TK_SIZEOF)) {
        return parseSizeOf();
    }
    
    if (match(TK_ZIS)) {
        return parseZis();
    }
    
    if (match(TK_LPAREN)) {
        ASTNode* expr = parseExpression();
        if (expr && match(TK_RPAREN)) {
            return expr;
        }
        if (expr) free(expr);
        errorAtCurrent("Expected ')' after expression");
        return NULL;
    }
    
    if (match(TK_LBRACKET)) {
        // Array literal
        ASTNode* node = newNode(NODE_JSON_ARR);
        // Simplified for now
        if (match(TK_RBRACKET)) {
            return node;
        }
        free(node);
        errorAtCurrent("Array parsing not fully implemented");
        return NULL;
    }
    
    if (match(TK_LBRACE)) {
        // Object literal
        ASTNode* node = newNode(NODE_JSON_OBJ);
        if (match(TK_RBRACE)) {
            return node;
        }
        free(node);
        errorAtCurrent("Object parsing not fully implemented");
        return NULL;
    }
    
    errorAtCurrent("Expected expression");
    return NULL;
}

static ASTNode* parseSizeOf() {
    if (!match(TK_LPAREN)) {
        errorAtCurrent("Expected '(' after sizeof");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_SIZEOF);
    if (!node) return NULL;
    
    node->data.size_op.expr = parseExpression();
    
    if (!match(TK_RPAREN)) {
        errorAtCurrent("Expected ')' after sizeof expression");
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parseZis() {
    if (!match(TK_LPAREN)) {
        errorAtCurrent("Expected '(' after zis");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_ZIS);
    if (!node) return NULL;
    
    node->data.size_op.expr = parseExpression();
    
    if (!match(TK_RPAREN)) {
        errorAtCurrent("Expected ')' after zis expression");
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parseUnary() {
    if (match(TK_MINUS) || match(TK_NOT)) {
        ASTNode* node = newNode(NODE_UNARY);
        if (!node) return NULL;
        
        node->data.int_val = previous.kind;
        node->left = parseUnary();
        return node;
    }
    
    return parsePrimary();
}

static int getPrecedence(TokenKind op) {
    switch (op) {
        case TK_EQ: case TK_NEQ: return 1;
        case TK_LT: case TK_LTE: case TK_GT: case TK_GTE: return 2;
        case TK_PLUS: case TK_MINUS: return 3;
        case TK_MULT: case TK_DIV: case TK_MOD: return 4;
        default: return 0;
    }
}

static ASTNode* parseBinary(ASTNode* left, int min_prec) {
    while (1) {
        TokenKind op = current.kind;
        int prec = getPrecedence(op);
        
        if (prec == 0 || prec < min_prec) break;
        
        advance();
        
        ASTNode* node = newNode(NODE_BINARY);
        if (!node) {
            free(left);
            return NULL;
        }
        
        node->data.int_val = op;
        node->left = left;
        node->right = parseUnary();
        
        if (!node->right) {
            free(node);
            free(left);
            return NULL;
        }
        
        left = node;
    }
    
    return left;
}

static ASTNode* parseTerm() {
    ASTNode* left = parseUnary();
    if (!left) return NULL;
    return parseBinary(left, 3);
}

static ASTNode* parseComparison() {
    ASTNode* left = parseTerm();
    if (!left) return NULL;
    return parseBinary(left, 2);
}

static ASTNode* parseEquality() {
    ASTNode* left = parseComparison();
    if (!left) return NULL;
    return parseBinary(left, 1);
}

static ASTNode* parseAssignment() {
    ASTNode* left = parseEquality();
    if (!left) return left;
    
    if (match(TK_ASSIGN)) {
        ASTNode* node = newNode(NODE_ASSIGN);
        if (!node) {
            free(left);
            return NULL;
        }
        
        if (left->type == NODE_IDENT) {
            node->data.name = left->data.name;
            free(left);
        } else {
            errorAtCurrent("Invalid assignment target");
            free(left);
            free(node);
            return NULL;
        }
        
        node->left = parseAssignment();
        return node;
    }
    
    return left;
}

static ASTNode* parseExpression() {
    return parseAssignment();
}

// ======================================================
// [SECTION] STATEMENT PARSING
// ======================================================
static ASTNode* parseStatement();
static ASTNode* parseBlock();
static ASTNode* parseVarDecl(TokenKind var_type);
static ASTNode* parsePrint();
static ASTNode* parseIf();
static ASTNode* parseWhile();
static ASTNode* parseFor();
static ASTNode* parseFuncDecl();
static ASTNode* parseClassDecl();
static ASTNode* parseMain();
static ASTNode* parseReturn();

static ASTNode* parseVarDecl(TokenKind var_type) {
    if (!match(TK_IDENT)) {
        errorAtCurrent("Expected variable name");
        return NULL;
    }
    
    char* var_name = str_copy(previous.value.str_val);
    ASTNode* node = NULL;
    
    switch (var_type) {
        case TK_VAR: node = newNode(NODE_VAR); break;
        case TK_NIP: node = newNode(NODE_NIP); break;
        case TK_SIM: node = newNode(NODE_SIM); break;
        case TK_NUUM: node = newNode(NODE_NUUM); break;
        default: node = newNode(NODE_VAR); break;
    }
    
    if (!node) {
        free(var_name);
        return NULL;
    }
    
    node->data.name = var_name;
    
    if (match(TK_ASSIGN)) {
        node->left = parseExpression();
    }
    
    if (!match(TK_SEMICOLON)) {
        errorAtCurrent("';' expected after variable declaration");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parsePrint() {
    if (!match(TK_LPAREN)) {
        errorAtCurrent("Expected '(' after print");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_PRINT);
    if (!node) return NULL;
    
    // Support multiple arguments
    if (current.kind != TK_RPAREN) {
        node->left = parseExpression();
        
        while (match(TK_COMMA)) {
            ASTNode* arg_node = newNode(NODE_PRINT);
            arg_node->left = parseExpression();
            // Chain print statements
            if (node->right) {
                ASTNode* last = node->right;
                while (last->right) last = last->right;
                last->right = arg_node;
            } else {
                node->right = arg_node;
            }
        }
    }
    
    if (!match(TK_RPAREN)) {
        errorAtCurrent("Expected ')' after print arguments");
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        errorAtCurrent("';' expected after print statement");
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parseIf() {
    if (!match(TK_LPAREN)) {
        errorAtCurrent("Expected '(' after if");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_IF);
    if (!node) return NULL;
    
    node->left = parseExpression();
    
    if (!match(TK_RPAREN)) {
        errorAtCurrent("Expected ')' after if condition");
        free(node);
        return NULL;
    }
    
    node->right = parseStatement();
    
    if (match(TK_ELSE)) {
        ASTNode* else_node = newNode(NODE_BLOCK);
        else_node->left = node->right;
        else_node->right = parseStatement();
        node->right = else_node;
    }
    
    return node;
}

static ASTNode* parseWhile() {
    if (!match(TK_LPAREN)) {
        errorAtCurrent("Expected '(' after while");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_WHILE);
    if (!node) return NULL;
    
    node->left = parseExpression();
    
    if (!match(TK_RPAREN)) {
        errorAtCurrent("Expected ')' after while condition");
        free(node);
        return NULL;
    }
    
    node->right = parseStatement();
    return node;
}

static ASTNode* parseFor() {
    // Simplified for loop: for (init; condition; increment) body
    if (!match(TK_LPAREN)) {
        errorAtCurrent("Expected '(' after for");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_FOR);
    if (!node) return NULL;
    
    // Parse init
    if (!match(TK_SEMICOLON)) {
        node->left = parseExpression(); // init
        if (!match(TK_SEMICOLON)) {
            errorAtCurrent("Expected ';' after for init");
            free(node);
            return NULL;
        }
    }
    
    // Parse condition
    ASTNode* cond = parseExpression();
    if (!match(TK_SEMICOLON)) {
        errorAtCurrent("Expected ';' after for condition");
        free(node);
        free(cond);
        return NULL;
    }
    
    // Parse increment
    ASTNode* incr = parseExpression();
    if (!match(TK_RPAREN)) {
        errorAtCurrent("Expected ')' after for increment");
        free(node);
        free(cond);
        free(incr);
        return NULL;
    }
    
    // Create condition and increment as a block
    if (cond || incr) {
        ASTNode* control = newNode(NODE_BLOCK);
        control->left = cond;
        control->right = incr;
        // Store in node's data temporarily
        node->data.func.body = control;
    }
    
    node->right = parseStatement();
    return node;
}

static ASTNode* parseFuncDecl() {
    if (!match(TK_IDENT)) {
        errorAtCurrent("Expected function name");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_FUNC);
    if (!node) return NULL;
    
    node->data.func.name = str_copy(previous.value.str_val);
    
    if (!match(TK_LPAREN)) {
        errorAtCurrent("Expected '(' after function name");
        free(node->data.func.name);
        free(node);
        return NULL;
    }
    
    // Parse parameters (simplified)
    if (!match(TK_RPAREN)) {
        // Skip parameters for now
        while (current.kind != TK_RPAREN && current.kind != TK_EOF) {
            advance();
        }
        if (!match(TK_RPAREN)) {
            errorAtCurrent("Expected ')' after parameters");
            free(node->data.func.name);
            free(node);
            return NULL;
        }
    }
    
    if (!match(TK_LBRACE)) {
        errorAtCurrent("Expected '{' for function body");
        free(node->data.func.name);
        free(node);
        return NULL;
    }
    
    node->data.func.body = parseBlock();
    
    if (!match(TK_RBRACE)) {
        errorAtCurrent("Expected '}' after function body");
        free(node->data.func.name);
        free(node->data.func.body);
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parseClassDecl() {
    if (!match(TK_IDENT)) {
        errorAtCurrent("Expected class name");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_CLASS);
    if (!node) return NULL;
    
    node->data.class.name = str_copy(previous.value.str_val);
    
    if (!match(TK_LBRACE)) {
        errorAtCurrent("Expected '{' after class name");
        free(node->data.class.name);
        free(node);
        return NULL;
    }
    
    // Parse class members
    node->data.class.fields = parseBlock();
    
    if (!match(TK_RBRACE)) {
        errorAtCurrent("Expected '}' after class body");
        free(node->data.class.name);
        free(node->data.class.fields);
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        errorAtCurrent("';' expected after class declaration");
        free(node->data.class.name);
        free(node->data.class.fields);
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parseMain() {
    if (!match(TK_LPAREN)) {
        errorAtCurrent("Expected '(' after main");
        return NULL;
    }
    
    if (!match(TK_RPAREN)) {
        errorAtCurrent("Expected ')'");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_MAIN);
    if (!node) return NULL;
    
    if (!match(TK_LBRACE)) {
        errorAtCurrent("Expected '{' for main body");
        free(node);
        return NULL;
    }
    
    node->left = parseBlock();
    
    if (!match(TK_RBRACE)) {
        errorAtCurrent("Expected '}' after main body");
        free(node->left);
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parseReturn() {
    ASTNode* node = newNode(NODE_RETURN);
    if (!node) return NULL;
    
    if (current.kind != TK_SEMICOLON) {
        node->left = parseExpression();
    }
    
    if (!match(TK_SEMICOLON)) {
        errorAtCurrent("';' expected after return");
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parseBlock() {
    ASTNode* node = newNode(NODE_BLOCK);
    ASTNode* current_node = node;
    
    while (current.kind != TK_RBRACE && current.kind != TK_EOF) {
        ASTNode* stmt = parseStatement();
        if (stmt) {
            if (!current_node->left) {
                current_node->left = stmt;
            } else if (!current_node->right) {
                current_node->right = stmt;
            } else {
                // Chain statements
                ASTNode* new_block = newNode(NODE_BLOCK);
                new_block->left = current_node->right;
                new_block->right = stmt;
                current_node->right = new_block;
            }
        } else {
            // Skip to next statement
            while (current.kind != TK_SEMICOLON && 
                   current.kind != TK_RBRACE && 
                   current.kind != TK_EOF) {
                advance();
            }
            if (current.kind == TK_SEMICOLON) advance();
        }
    }
    
    return node;
}

static ASTNode* parseStatement() {
    if (match(TK_VAR)) return parseVarDecl(TK_VAR);
    if (match(TK_NIP)) return parseVarDecl(TK_NIP);
    if (match(TK_SIM)) return parseVarDecl(TK_SIM);
    if (match(TK_NUUM)) return parseVarDecl(TK_NUUM);
    if (match(TK_PRINT)) return parsePrint();
    if (match(TK_IF)) return parseIf();
    if (match(TK_WHILE)) return parseWhile();
    if (match(TK_FOR)) return parseFor();
    if (match(TK_FUNC)) return parseFuncDecl();
    if (match(TK_CLASS)) return parseClassDecl();
    if (match(TK_MAIN)) return parseMain();
    if (match(TK_RETURN)) return parseReturn();
    if (match(TK_IMPORT)) return parseImportStatement();
    
    // Expression statement
    if (current.kind == TK_IDENT) {
        char* name = str_copy(current.value.str_val);
        advance();
        
        if (current.kind == TK_ASSIGN) {
            return parseAssignment(name);
        }
        
        // Function call or other expression
        free(name);
        ASTNode* expr = parseExpression();
        if (expr && match(TK_SEMICOLON)) {
            return expr;
        }
        if (expr) free(expr);
        errorAtCurrent("';' expected after expression");
        return NULL;
    }
    
    // Empty statement
    if (match(TK_SEMICOLON)) {
        return newNode(NODE_BLOCK); // Empty block
    }
    
    errorAtCurrent("Expected statement");
    return NULL;
}

// ======================================================
// [SECTION] MAIN PARSER FUNCTION
// ======================================================
ASTNode** parse(const char* source, int* count) {
    initLexer(source);
    advance();
    hadError = false;
    
    int capacity = 10;
    ASTNode** nodes = malloc(capacity * sizeof(ASTNode*));
    *count = 0;
    
    if (!nodes) return NULL;
    
    while (current.kind != TK_EOF) {
        if (*count >= capacity) {
            capacity *= 2;
            ASTNode** new_nodes = realloc(nodes, capacity * sizeof(ASTNode*));
            if (!new_nodes) break;
            nodes = new_nodes;
        }
        
        ASTNode* node = parseStatement();
        if (node) {
            nodes[(*count)++] = node;
        } else if (hadError) {
            // Skip to next statement on error
            while (current.kind != TK_SEMICOLON && current.kind != TK_EOF) {
                advance();
            }
            if (current.kind == TK_SEMICOLON) advance();
            hadError = false;
        }
    }
    
    return nodes;
}

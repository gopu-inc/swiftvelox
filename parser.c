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
    // Syntaxe: import() {"module"};
    //          import() {"./file.swf"};
    //          import() {"PIL", from "module"};
    
    if (!match(TK_LPAREN)) {
        printf(RED "[ERROR]" RESET " Expected '(' after import\n");
        return NULL;
    }
    
    if (!match(TK_RPAREN)) {
        printf(RED "[ERROR]" RESET " Expected ')' after '('\n");
        return NULL;
    }
    
    if (!match(TK_LBRACE)) {
        printf(RED "[ERROR]" RESET " Expected '{' for import list\n");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_IMPORT);
    if (!node) return NULL;
    
    node->from_module = NULL;
    
    // Parse import list
    char** imports = NULL;
    int count = 0;
    int capacity = 4;
    
    imports = malloc(capacity * sizeof(char*));
    if (!imports) {
        free(node);
        return NULL;
    }
    
    // Premier import
    if (match(TK_STRING)) {
        imports[count++] = str_copy(previous.value.str_val);
    } else {
        printf(RED "[ERROR]" RESET " Expected string in import list\n");
        free(imports);
        free(node);
        return NULL;
    }
    
    // Imports supplémentaires ou 'from'
    while (match(TK_COMMA)) {
        // Vérifier 'from'
        if (current.kind == TK_IDENT && 
            current.value.str_val && 
            strcmp(current.value.str_val, "from") == 0) {
            
            advance(); // Consommer 'from'
            
            if (!match(TK_STRING)) {
                printf(RED "[ERROR]" RESET " Expected module name after 'from'\n");
                break;
            }
            
            node->from_module = str_copy(previous.value.str_val);
            break;
        }
        
        // Autre import
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
            printf(RED "[ERROR]" RESET " Expected string after comma\n");
            break;
        }
    }
    
    if (!match(TK_RBRACE)) {
        printf(RED "[ERROR]" RESET " Expected '}' after import list\n");
        for (int i = 0; i < count; i++) free(imports[i]);
        free(imports);
        free(node->from_module);
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        printf(RED "[ERROR]" RESET " >> ';' expected after import statement\n");
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
static ASTNode* parsePrimary();
static ASTNode* parseExpression();

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
    
    if (match(TK_TRUE) || match(TK_FALSE)) {
        ASTNode* node = newNode(NODE_INT);
        if (node) node->data.int_val = (previous.kind == TK_TRUE) ? 1 : 0;
        return node;
    }
    
    if (match(TK_IDENT)) {
        ASTNode* node = newNode(NODE_IDENT);
        if (node) node->data.name = str_copy(previous.value.str_val);
        return node;
    }
    
    if (match(TK_LPAREN)) {
        ASTNode* expr = parseExpression();
        if (expr) {
            if (match(TK_RPAREN)) {
                return expr;
            }
            free(expr);
        }
        return NULL;
    }
    
    return NULL;
}

static int getPrecedence(TokenKind op) {
    switch (op) {
        case TK_EQ: case TK_NEQ: case TK_LT: case TK_GT: case TK_LTE: case TK_GTE:
            return 1;
        case TK_PLUS: case TK_MINUS:
            return 2;
        case TK_MULT: case TK_DIV: case TK_MOD:
            return 3;
        default:
            return 0;
    }
}

static ASTNode* parseBinary(int min_prec) {
    ASTNode* left = parsePrimary();
    if (!left) return NULL;
    
    while (1) {
        TokenKind op = current.kind;
        int prec = getPrecedence(op);
        
        if (prec == 0 || prec < min_prec) break;
        
        advance(); // Consommer l'opérateur
        
        ASTNode* node = newNode(NODE_BINARY);
        if (!node) {
            free(left);
            return NULL;
        }
        
        node->data.int_val = op;
        node->left = left;
        node->right = parseBinary(prec + 1);
        
        if (!node->right) {
            free(node);
            free(left);
            return NULL;
        }
        
        left = node;
    }
    
    return left;
}

static ASTNode* parseExpression() {
    return parseBinary(0);
}

// ======================================================
// [SECTION] STATEMENT PARSING
// ======================================================
static ASTNode* parseStatement();

static ASTNode* parseVarDecl() {
    if (!match(TK_IDENT)) {
        printf(RED "[ERROR]" RESET " Expected variable name\n");
        return NULL;
    }
    
    char* var_name = str_copy(previous.value.str_val);
    ASTNode* node = newNode(NODE_VAR);
    if (!node) {
        free(var_name);
        return NULL;
    }
    
    node->data.name = var_name;
    
    if (match(TK_ASSIGN)) {
        node->left = parseExpression();
    }
    
    if (!match(TK_SEMICOLON)) {
        printf(RED "[ERROR]" RESET " >> ';' expected after variable declaration\n");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parsePrint() {
    if (!match(TK_LPAREN)) {
        printf(RED "[ERROR]" RESET " Expected '(' after print\n");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_PRINT);
    if (!node) return NULL;
    
    node->left = parseExpression();
    
    if (!match(TK_RPAREN)) {
        printf(RED "[ERROR]" RESET " Expected ')' after expression\n");
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        printf(RED "[ERROR]" RESET " >> ';' expected after print statement\n");
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parseAssignment(char* name) {
    if (!match(TK_ASSIGN)) {
        printf(RED "[ERROR]" RESET " Expected '=' after identifier\n");
        free(name);
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_ASSIGN);
    if (!node) {
        free(name);
        return NULL;
    }
    
    node->data.name = name;
    node->left = parseExpression();
    
    if (!match(TK_SEMICOLON)) {
        printf(RED "[ERROR]" RESET " >> ';' expected after assignment\n");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    return node;
}

static ASTNode* parseStatement() {
    if (match(TK_VAR)) {
        return parseVarDecl();
    }
    
    if (match(TK_PRINT)) {
        return parsePrint();
    }
    
    if (match(TK_IMPORT)) {
        return parseImportStatement();
    }
    
    // Assignment
    if (current.kind == TK_IDENT) {
        char* name = str_copy(current.value.str_val);
        advance();
        
        if (current.kind == TK_ASSIGN) {
            return parseAssignment(name);
        }
        
        free(name);
        printf(RED "[ERROR]" RESET " Identifier without assignment not supported\n");
        return NULL;
    }
    
    return NULL;
}

// ======================================================
// [SECTION] MAIN PARSER FUNCTION
// ======================================================
ASTNode** parse(const char* source, int* count) {
    initLexer(source);
    advance();
    
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
        } else {
            // Skip to next statement
            while (current.kind != TK_SEMICOLON && current.kind != TK_EOF) {
                advance();
            }
            if (current.kind == TK_SEMICOLON) advance();
        }
    }
    
    return nodes;
}

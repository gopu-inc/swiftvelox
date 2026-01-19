#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "common.h"

extern Token scanToken();
extern void initLexer(const char* source);

// ======================================================
// [SECTION] PARSER STATE
// ======================================================
static Token current;
static Token previous;
static bool hadError = false;
static bool panicMode = false;

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
    return peekNextToken().kind == kind;
}

static Token peekNextToken() {
    // Save current state
    const char* saved_current = lexer.current;
    int saved_line = lexer.line;
    int saved_column = lexer.column;
    
    Token next = scanToken();
    
    // Restore state
    lexer.current = saved_current;
    lexer.line = saved_line;
    lexer.column = saved_column;
    
    return next;
}

static void errorAt(Token token, const char* message) {
    if (panicMode) return;
    panicMode = true;
    
    fprintf(stderr, RED "[PARSER ERROR]" RESET " Line %d, Col %d: %s\n", 
            token.line, token.column, message);
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
            case TK_RETURN:
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
static ASTNode* logicOr();
static ASTNode* logicAnd();
static ASTNode* equality();
static ASTNode* comparison();
static ASTNode* term();
static ASTNode* factor();
static ASTNode* unary();
static ASTNode* call();
static ASTNode* primary();

static ASTNode* expression() {
    return assignment();
}

static ASTNode* assignment() {
    ASTNode* expr = logicOr();
    
    if (match(TK_ASSIGN) || match(TK_PLUS_ASSIGN) || 
        match(TK_MINUS_ASSIGN) || match(TK_MULT_ASSIGN) ||
        match(TK_DIV_ASSIGN) || match(TK_MOD_ASSIGN)) {
        
        TokenKind op = previous.kind;
        ASTNode* value = assignment();
        
        if (expr->type != NODE_IDENT) {
            error("Invalid assignment target");
            return expr;
        }
        
        ASTNode* node = newNode(NODE_ASSIGN);
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
    
    while (match(TK_EQ) || match(TK_NEQ) || match(TK_SPACESHIP)) {
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
    
    while (match(TK_GT) || match(TK_GTE) || match(TK_LT) || match(TK_LTE)) {
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
    ASTNode* expr = primary();
    
    while (true) {
        if (match(TK_LPAREN)) {
            // Function call
            ASTNode* node = newNode(NODE_FUNC_CALL);
            if (node) {
                node->left = expr;
                
                // Parse arguments
                if (!check(TK_RPAREN)) {
                    do {
                        // TODO: Add arguments
                    } while (match(TK_COMMA));
                }
                
                if (!match(TK_RPAREN)) {
                    error("Expected ')' after arguments");
                }
                
                expr = node;
            }
        } else if (match(TK_PERIOD)) {
            // Member access
            if (!match(TK_IDENT)) {
                error("Expected member name after '.'");
                break;
            }
            
            ASTNode* node = newNode(NODE_MEMBER_ACCESS);
            if (node) {
                node->left = expr;
                node->data.name = str_copy(previous.value.str_val);
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
    if (match(TK_NULL)) return newNode(NODE_NULL);
    if (match(TK_UNDEFINED)) return newNode(NODE_UNDEFINED);
    
    if (match(TK_INT)) return newIntNode(previous.value.int_val);
    if (match(TK_FLOAT)) return newFloatNode(previous.value.float_val);
    if (match(TK_STRING)) return newStringNode(previous.value.str_val);
    
    if (match(TK_IDENT)) return newIdentNode(previous.value.str_val);
    
    if (match(TK_LPAREN)) {
        ASTNode* expr = expression();
        if (!match(TK_RPAREN)) {
            error("Expected ')' after expression");
        }
        return expr;
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
    
    error("Expected expression");
    return NULL;
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
static ASTNode* returnStatement();
static ASTNode* printStatement();
static ASTNode* variableDeclaration();

static ASTNode* statement() {
    if (match(TK_PRINT)) return printStatement();
    if (match(TK_IF)) return ifStatement();
    if (match(TK_WHILE)) return whileStatement();
    if (match(TK_FOR)) return forStatement();
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
    
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        ASTNode* stmt = declaration();
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
    
    node->left = expression(); // Condition
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after if condition");
        free(node);
        return NULL;
    }
    
    node->right = statement(); // Then branch
    
    if (match(TK_ELSE)) {
        node->third = statement(); // Else branch
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
    
    node->right = statement(); // Body
    
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
        node->left = variableDeclaration();
    } else {
        node->left = expressionStatement();
    }
    
    // Condition
    if (!check(TK_SEMICOLON)) {
        node->extra = expression(); // Condition stored in extra
    }
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after loop condition");
    }
    
    // Increment
    if (!check(TK_RPAREN)) {
        node->third = expression(); // Increment stored in third
    }
    if (!match(TK_RPAREN)) {
        error("Expected ')' after for clauses");
    }
    
    node->right = statement(); // Body
    
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
    
    // Optional additional arguments
    while (match(TK_COMMA)) {
        // TODO: Handle multiple print arguments
        expression();
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after print arguments");
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after print statement");
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
    
    // Optional type annotation
    if (match(TK_COLON)) {
        if (match(TK_IDENT)) {
            node->data.type_name = str_copy(previous.value.str_val);
        } else {
            error("Expected type name after ':'");
        }
    }
    
    // Optional initialization
    if (match(TK_ASSIGN) || match(TK_DARROW) || match(TK_LDARROW) || match(TK_RDARROW)) {
        node->left = expression();
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after variable declaration");
    }
    
    return node;
}

// ======================================================
// [SECTION] FUNCTION & CLASS DECLARATION
// ======================================================
static ASTNode* functionDeclaration() {
    if (!match(TK_IDENT)) {
        error("Expected function name");
        return NULL;
    }
    
    char* funcName = str_copy(previous.value.str_val);
    ASTNode* node = newNode(NODE_FUNC_DECL);
    if (!node) {
        free(funcName);
        return NULL;
    }
    
    node->data.func.name = funcName;
    
    // Parameters
    if (!match(TK_LPAREN)) {
        error("Expected '(' after function name");
        free(funcName);
        free(node);
        return NULL;
    }
    
    if (!match(TK_RPAREN)) {
        // TODO: Parse parameters
        if (!match(TK_RPAREN)) {
            error("Expected ')' after parameters");
        }
    }
    
    // Return type
    if (match(TK_COLON)) {
        if (match(TK_IDENT)) {
            node->data.func.return_type = str_copy(previous.value.str_val);
        } else {
            error("Expected return type after ':'");
        }
    }
    
    // Function body
    if (!match(TK_LBRACE)) {
        error("Expected '{' before function body");
    }
    
    node->data.func.body = block();
    
    return node;
}

static ASTNode* classDeclaration() {
    if (!match(TK_IDENT)) {
        error("Expected class name");
        return NULL;
    }
    
    char* className = str_copy(previous.value.str_val);
    ASTNode* node = newNode(NODE_CLASS);
    if (!node) {
        free(className);
        return NULL;
    }
    
    node->data.class_def.name = className;
    
    // Inheritance
    if (match(TK_COLON)) {
        if (match(TK_IDENT)) {
            node->data.class_def.parent_class = str_copy(previous.value.str_val);
        } else {
            error("Expected parent class name after ':'");
        }
    }
    
    if (!match(TK_LBRACE)) {
        error("Expected '{' before class body");
    }
    
    // Parse class members
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        // Parse member visibility
        char* visibility = "private";
        if (match(TK_PUBLIC)) visibility = "public";
        else if (match(TK_PRIVATE)) visibility = "private";
        else if (match(TK_PROTECTED)) visibility = "protected";
        
        // Parse member
        if (match(TK_VAR) || match(TK_LET) || match(TK_CONST) ||
            match(TK_NET) || match(TK_CLOG) || match(TK_DOS) || match(TK_SEL)) {
            // TODO: Add member to class
        } else if (match(TK_FUNC)) {
            // Method
            // TODO: Parse method
        } else {
            error("Expected member declaration in class");
            break;
        }
    }
    
    if (!match(TK_RBRACE)) {
        error("Expected '}' after class body");
    }
    
    return node;
}

// ======================================================
// [SECTION] IMPORT & MODULE DECLARATION
// ======================================================
static ASTNode* importDeclaration() {
    if (!match(TK_LPAREN)) {
        error("Expected '(' after import");
        return NULL;
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after '('");
        return NULL;
    }
    
    if (!match(TK_LBRACE)) {
        error("Expected '{' for import list");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_IMPORT);
    if (!node) return NULL;
    
    // Parse import list
    char** imports = NULL;
    int count = 0;
    int capacity = 4;
    
    imports = malloc(capacity * sizeof(char*));
    if (!imports) {
        free(node);
        return NULL;
    }
    
    // First import
    if (match(TK_STRING)) {
        imports[count++] = str_copy(previous.value.str_val);
    } else {
        error("Expected string in import list");
        free(imports);
        free(node);
        return NULL;
    }
    
    // Additional imports or 'from'
    while (match(TK_COMMA)) {
        // Check for 'from'
        if (check(TK_FROM)) {
            advance(); // Consume 'from'
            
            if (!match(TK_STRING)) {
                error("Expected module name after 'from'");
                break;
            }
            
            node->data.imports.from_module = str_copy(previous.value.str_val);
            break;
        }
        
        // Another import
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
            error("Expected string after comma");
            break;
        }
    }
    
    if (!match(TK_RBRACE)) {
        error("Expected '}' after import list");
        for (int i = 0; i < count; i++) free(imports[i]);
        free(imports);
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after import statement");
        for (int i = 0; i < count; i++) free(imports[i]);
        free(imports);
        free(node);
        return NULL;
    }
    
    node->data.imports.modules = imports;
    node->data.imports.module_count = count;
    return node;
}

// ======================================================
// [SECTION] TYPELOCK DECLARATION
// ======================================================
static ASTNode* typelockDeclaration() {
    if (!match(TK_LBRACE)) {
        error("Expected '{' after typelock");
        return NULL;
    }
    
    ASTNode* node = newNode(NODE_TYPELOCK);
    
    // Parse typelock body
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        // TODO: Parse typelock members (type definitions)
        advance();
    }
    
    if (!match(TK_RBRACE)) {
        error("Expected '}' after typelock body");
    }
    
    return node;
}

// ======================================================
// [SECTION] MAIN DECLARATION
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
    
    // Main body
    if (!match(TK_LBRACE)) {
        error("Expected '{' before main body");
        free(node);
        return NULL;
    }
    
    node->left = block();
    
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
// [SECTION] DECLARATION PARSING
// ======================================================
static ASTNode* declaration() {
    if (match(TK_CLASS)) return classDeclaration();
    if (match(TK_FUNC)) return functionDeclaration();
    if (match(TK_IMPORT)) return importDeclaration();
    if (match(TK_TYPELOCK)) return typelockDeclaration();
    if (match(TK_MAIN)) return mainDeclaration();
    if (match(TK_DBVAR)) return dbvarCommand();
    
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
        
        ASTNode* node = declaration();
        if (node) {
            nodes[(*count)++] = node;
        }
        
        if (panicMode) {
            synchronize();
        }
    }
    
    return nodes;
}

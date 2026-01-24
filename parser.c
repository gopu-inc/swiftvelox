#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <unistd.h>
#include "common.h"

extern void execute(ASTNode* node);
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
static int warningCount = 0;
static int scope_level = 0;

// ======================================================
// [SECTION] ERROR HANDLING
// ======================================================
static void errorAt(Token token, const char* message) {
    if (panicMode) return;
    
    fprintf(stderr, "%s[PARSER ERROR]%s Line %d, Col %d: %s\n", 
            COLOR_RED, COLOR_RESET, token.line, token.column, message);
    
    if (token.start) {
        fprintf(stderr, "  Token: '%.*s' (type: %d)\n", 
                token.length, token.start, token.kind);
    }
    
    errorCount++;
    panicMode = true;
    
    if (errorCount >= 3) {
        fprintf(stderr, "%s[PARSER FATAL]%s Too many errors (%d). Stopping.\n", 
                COLOR_BRIGHT_RED, COLOR_RESET, errorCount);
        exit(1);
    }
}

static void error(const char* message) {
    errorAt(previous, message);
}

static void errorAtCurrent(const char* message) {
    errorAt(current, message);
}

static void warningAt(Token token, const char* message) {
    warningCount++;
    fprintf(stderr, "%s[PARSER WARNING]%s Line %d, Col %d: %s\n", 
            COLOR_YELLOW, COLOR_RESET, token.line, token.column, message);
}

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

static Token consume(TokenKind kind, const char* message) {
    if (check(kind)) {
        advance();
        return previous;
    }
    
    errorAtCurrent(message);
    return previous;
}

static void synchronize() {
    panicMode = false;
    
    fprintf(stderr, "%s[PARSER]%s Synchronizing...\n", COLOR_YELLOW, COLOR_RESET);
    
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
            case TK_TYPE:
            case TK_TRY:
            case TK_THROW:
            case TK_SWITCH:
            case TK_CASE:
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
    if (!node) {
        fprintf(stderr, "%s[PARSER FATAL]%s Memory allocation failed for AST node\n", 
                COLOR_BRIGHT_RED, COLOR_RESET);
        exit(1);
    }
    
    node->type = type;
    node->line = previous.line;
    node->column = previous.column;
    
    return node;
}

static ASTNode* newIntNode(int64_t value) {
    ASTNode* node = newNode(NODE_INT);
    node->data.int_val = value;
    return node;
}

static ASTNode* newFloatNode(double value) {
    ASTNode* node = newNode(NODE_FLOAT);
    node->data.float_val = value;
    return node;
}

static ASTNode* newStringNode(char* value) {
    ASTNode* node = newNode(NODE_STRING);
    node->data.str_val = str_copy(value);
    return node;
}

static ASTNode* newBoolNode(bool value) {
    ASTNode* node = newNode(NODE_BOOL);
    node->data.bool_val = value;
    return node;
}

static ASTNode* newIdentNode(char* name) {
    ASTNode* node = newNode(NODE_IDENT);
    node->data.name = str_copy(name);
    return node;
}

// ======================================================
// [SECTION] EXPRESSION PARSING - COMPLETE
// ======================================================
static ASTNode* expression();
static ASTNode* assignment();
static ASTNode* ternary();
static ASTNode* logicOr();
static ASTNode* logicAnd();
static ASTNode* bitwiseOr();
static ASTNode* bitwiseXor();
static ASTNode* bitwiseAnd();
static ASTNode* equality();
static ASTNode* comparison();
static ASTNode* bitshift();
static ASTNode* term();
static ASTNode* factor();
static ASTNode* unary();
static ASTNode* call();
static ASTNode* memberAccess();
static ASTNode* primary();
static ASTNode* lambdaExpression();

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
        match(TK_POW_ASSIGN) || match(TK_CONCAT_ASSIGN) ||
        match(TK_BIT_AND) || match(TK_BIT_OR) || match(TK_BIT_XOR) ||
        match(TK_SHL) || match(TK_SHR) || match(TK_USHR)) {
        
        TokenKind op = previous.kind;
        
        // Check for compound assignment operators
        bool is_compound = false;
        if (op == TK_PLUS_ASSIGN || op == TK_MINUS_ASSIGN || 
            op == TK_MULT_ASSIGN || op == TK_DIV_ASSIGN ||
            op == TK_MOD_ASSIGN || op == TK_POW_ASSIGN ||
            op == TK_CONCAT_ASSIGN) {
            is_compound = true;
        }
        
        ASTNode* value = assignment();
        
        if (expr->type != NODE_IDENT && 
            expr->type != NODE_MEMBER_ACCESS &&
            expr->type != NODE_ARRAY_ACCESS) {
            error("Invalid assignment target");
            return expr;
        }
        
        ASTNode* node = is_compound ? newNode(NODE_COMPOUND_ASSIGN) : newNode(NODE_ASSIGN);
        if (node) {
            node->left = expr;
            node->right = value;
            node->op_type = op;
            
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
        consume(TK_COLON, "Expected ':' in ternary operator");
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

// Logic OR
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

// Logic AND
static ASTNode* logicAnd() {
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

// Bitwise OR
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

// Bitwise XOR
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

// Bitwise AND
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

// Equality operators
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

// Comparison operators
static ASTNode* comparison() {
    ASTNode* expr = bitshift();
    
    while (match(TK_GT) || match(TK_GTE) || match(TK_LT) || match(TK_LTE) || match(TK_IN)) {
        TokenKind op = previous.kind;
        ASTNode* node = newNode(NODE_BINARY);
        if (node) {
            node->op_type = op;
            node->left = expr;
            node->right = bitshift();
            expr = node;
        }
    }
    
    return expr;
}

// Bit shift operators
static ASTNode* bitshift() {
    ASTNode* expr = term();
    
    while (match(TK_SHL) || match(TK_SHR) || match(TK_USHR)) {
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

// Addition and subtraction
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

// Multiplication, division, modulo, power
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

// Unary operators
static ASTNode* unary() {
    if (match(TK_MINUS) || match(TK_NOT) || match(TK_BIT_NOT) || 
        match(TK_PLUS) || match(TK_TYPEOF) || match(TK_AWAIT)) {
        TokenKind op = previous.kind;
        ASTNode* node = newNode(NODE_UNARY);
        if (node) {
            node->op_type = op;
            node->left = unary();
            return node;
        }
    }
    
    // Check for increment/decrement prefix
    if (match(TK_INCREMENT) || match(TK_DECREMENT)) {
        TokenKind op = previous.kind;
        ASTNode* node = newNode(NODE_UNARY);
        if (node) {
            node->op_type = op;
            node->left = call(); // Can only apply to l-values
            return node;
        }
    }
    
    return call();
}

// Lambda expressions
static ASTNode* lambdaExpression() {
    if (match(TK_LAMBDA)) {
        ASTNode* node = newNode(NODE_LAMBDA);
        
        // Parse parameters
        if (match(TK_LPAREN)) {
            if (!match(TK_RPAREN)) {
                // Parse parameters
                ASTNode* first_param = NULL;
                ASTNode* current_param = NULL;
                
                if (match(TK_IDENT)) {
                    first_param = newIdentNode(previous.value.str_val);
                    current_param = first_param;
                    
                    while (match(TK_COMMA)) {
                        if (!match(TK_IDENT)) {
                            error("Expected parameter name after comma");
                            break;
                        }
                        ASTNode* param = newIdentNode(previous.value.str_val);
                        if (current_param) {
                            current_param->right = param;
                            current_param = param;
                        }
                    }
                }
                
                consume(TK_RPAREN, "Expected ')' after lambda parameters");
                node->left = first_param;
            }
        } else if (match(TK_IDENT)) {
            // Single parameter without parentheses
            node->left = newIdentNode(previous.value.str_val);
        }
        
        // Parse body
        if (match(TK_RARROW) || match(TK_DARROW)) {
            node->op_type = previous.kind;
            node->right = expression();
        } else {
            error("Expected '->' or '=>' after lambda parameters");
        }
        
        return node;
    }
    
    return NULL;
}

// Function calls
static ASTNode* call() {
    ASTNode* expr = memberAccess();
    
    if (match(TK_LPAREN)) {
        ASTNode* node = newNode(NODE_FUNC_CALL);
        if (!node) return expr;
        
        if (expr->type == NODE_IDENT && expr->data.name) {
            node->data.name = str_copy(expr->data.name);
        }
        
        // Parse arguments
        ASTNode* args = NULL;
        ASTNode* current_arg = NULL;
        
        if (!check(TK_RPAREN)) {
            args = expression();
            current_arg = args;
            
            while (match(TK_COMMA)) {
                ASTNode* next_arg = expression();
                if (current_arg) {
                    current_arg->right = next_arg;
                    current_arg = next_arg;
                }
            }
        }
        
        consume(TK_RPAREN, "Expected ')' after arguments");
        
        // Cleanup
        if (expr->type == NODE_IDENT) {
            free(expr->data.name);
        }
        free(expr);
        
        node->left = args;
        return node;
    }
    
    return expr;
}

// Member access (a.b, a?.b, a[b])
static ASTNode* memberAccess() {
    ASTNode* expr = primary();
    
    while (match(TK_PERIOD) || match(TK_LBRACKET) || match(TK_SAFE_NAV)) {
        TokenKind op = previous.kind;
        
        if (op == TK_PERIOD || op == TK_SAFE_NAV) {
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
            ASTNode* index = expression();
            consume(TK_RBRACKET, "Expected ']' after index");
            
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

// Primary expressions
static ASTNode* primary() {
    // Try lambda first
    ASTNode* lambda = lambdaExpression();
    if (lambda) return lambda;
    
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
    
    // Sizeof operator
    if (match(TK_SIZEOF) || match(TK_SIZE) || match(TK_SIZ)) {
        consume(TK_LPAREN, "Expected '(' after size");
        
        ASTNode* node = newNode(NODE_SIZEOF);
        if (node) {
            if (match(TK_IDENT)) {
                node->data.size_info.var_name = str_copy(previous.value.str_val);
            } else {
                error("Expected identifier in size()");
                free(node);
                return NULL;
            }
            
            consume(TK_RPAREN, "Expected ')' after size()");
        }
        return node;
    }
    
    // New operator
    if (match(TK_NEW)) {
        ASTNode* node = newNode(NODE_NEW);
        if (!match(TK_IDENT)) {
            error("Expected class name after 'new'");
            free(node);
            return NULL;
        }
        
        node->data.name = str_copy(previous.value.str_val);
        
        if (match(TK_LPAREN)) {
            // Parse constructor arguments
            ASTNode* args = NULL;
            ASTNode* current_arg = NULL;
            
            if (!check(TK_RPAREN)) {
                args = expression();
                current_arg = args;
                
                while (match(TK_COMMA)) {
                    ASTNode* next_arg = expression();
                    if (current_arg) {
                        current_arg->right = next_arg;
                        current_arg = next_arg;
                    }
                }
            }
            
            consume(TK_RPAREN, "Expected ')' after constructor arguments");
            node->left = args;
        }
        
        return node;
    }
    
    // Delete operator
    if (match(TK_DELETE)) {
        ASTNode* node = newNode(NODE_DELETE);
        node->left = unary();
        return node;
    }
    
    // Spread operator
    if (match(TK_SPREAD)) {
        ASTNode* node = newNode(NODE_UNARY);
        node->op_type = TK_SPREAD;
        node->left = primary();
        return node;
    }
    
    // Parenthesized expression
    if (match(TK_LPAREN)) {
        ASTNode* expr = expression();
        consume(TK_RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    // List/Array literal
    if (match(TK_LBRACKET)) {
        ASTNode* node = newNode(NODE_LIST);
        ASTNode* current_elem = NULL;
        
        if (!check(TK_RBRACKET)) {
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
        }
        
        consume(TK_RBRACKET, "Expected ']' after list");
        return node;
    }
    
    // Object/Map literal
    if (match(TK_LBRACE)) {
        ASTNode* node = newNode(NODE_MAP);
        ASTNode* first_pair = NULL;
        ASTNode* current_pair = NULL;
        
        if (!check(TK_RBRACE)) {
            do {
                char* key = NULL;
                if (match(TK_STRING)) {
                    key = str_copy(previous.value.str_val);
                } else if (match(TK_IDENT)) {
                    key = str_copy(previous.value.str_val);
                } else {
                    error("Expected string or identifier as object key");
                    break;
                }
                
                consume(TK_COLON, "Expected ':' after object key");
                
                ASTNode* value = expression();
                
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
        }
        
        consume(TK_RBRACE, "Expected '}' after object");
        node->left = first_pair;
        return node;
    }
    
    // JSON literal
    if (match(TK_JSON)) {
        consume(TK_STRING, "Expected JSON string after 'json'");
        
        ASTNode* node = newNode(NODE_JSON);
        if (node) {
            node->data.data_literal.data = str_copy(previous.value.str_val);
            node->data.data_literal.format = str_copy("json");
        }
        return node;
    }
    
    // XML literal
    if (match(TK_XML)) {
        consume(TK_STRING, "Expected XML string after 'xml'");
        
        ASTNode* node = newNode(NODE_XML);
        if (node) {
            node->data.data_literal.data = str_copy(previous.value.str_val);
            node->data.data_literal.format = str_copy("xml");
        }
        return node;
    }
    
    // YAML literal
    if (match(TK_YAML)) {
        consume(TK_STRING, "Expected YAML string after 'yaml'");
        
        ASTNode* node = newNode(NODE_YAML);
        if (node) {
            node->data.data_literal.data = str_copy(previous.value.str_val);
            node->data.data_literal.format = str_copy("yaml");
        }
        return node;
    }
    
    errorAtCurrent("Expected expression");
    return NULL;
}

// ======================================================
// [SECTION] STATEMENT PARSING - COMPLETE
// ======================================================
static ASTNode* statement();
static ASTNode* declaration();
static ASTNode* block();
static ASTNode* ifStatement();
static ASTNode* whileStatement();
static ASTNode* doWhileStatement();
static ASTNode* forStatement();
static ASTNode* forInStatement();
static ASTNode* switchStatement();
static ASTNode* caseStatement();
static ASTNode* returnStatement();
static ASTNode* yieldStatement();
static ASTNode* breakStatement();
static ASTNode* continueStatement();
static ASTNode* throwStatement();
static ASTNode* tryStatement();
static ASTNode* printStatement();
static ASTNode* weldStatement();
static ASTNode* readStatement();
static ASTNode* writeStatement();
static ASTNode* assertStatement();
static ASTNode* passStatement();
static ASTNode* withStatement();
static ASTNode* learnStatement();
static ASTNode* lockStatement();
static ASTNode* appendStatement();
static ASTNode* pushStatement();
static ASTNode* popStatement();
static ASTNode* expressionStatement();
static ASTNode* variableDeclaration();
static ASTNode* functionDeclaration(bool is_exported);
static ASTNode* classDeclaration();
static ASTNode* structDeclaration();
static ASTNode* enumDeclaration();
static ASTNode* interfaceDeclaration();
static ASTNode* typedefDeclaration();
static ASTNode* namespaceDeclaration();
static ASTNode* importStatement();
static ASTNode* exportStatement();
static ASTNode* mainDeclaration();
static ASTNode* dbvarStatement();




// ======================================================
// [SECTION] IO STATEMENT PARSING
// ======================================================

static ASTNode* ioOpenStatement() {
    ASTNode* node = newNode(NODE_FILE_OPEN);
    
    consume(TK_LPAREN, "Expected '(' after io.open");
    node->left = expression(); // filename
    consume(TK_COMMA, "Expected ',' after filename");
    node->right = expression(); // mode
    
    if (match(TK_COMMA)) {
        node->third = expression(); // variable pour stocker fd
    }
    
    consume(TK_RPAREN, "Expected ')' after io.open arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.open");
    
    return node;
}

static ASTNode* ioCloseStatement() {
    ASTNode* node = newNode(NODE_FILE_CLOSE);
    
    consume(TK_LPAREN, "Expected '(' after io.close");
    node->left = expression(); // fd
    consume(TK_RPAREN, "Expected ')' after io.close arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.close");
    
    return node;
}

static ASTNode* ioReadStatement() {
    ASTNode* node = newNode(NODE_FILE_READ);
    
    consume(TK_LPAREN, "Expected '(' after io.read");
    node->left = expression(); // fd
    
    if (match(TK_COMMA)) {
        node->right = expression(); // size
        if (match(TK_COMMA)) {
            node->third = expression(); // variable pour résultat
        }
    }
    
    consume(TK_RPAREN, "Expected ')' after io.read arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.read");
    
    return node;
}

static ASTNode* ioWriteStatement() {
    ASTNode* node = newNode(NODE_FILE_WRITE);
    
    consume(TK_LPAREN, "Expected '(' after io.write");
    node->left = expression(); // fd
    consume(TK_COMMA, "Expected ',' after fd");
    node->right = expression(); // data
    
    consume(TK_RPAREN, "Expected ')' after io.write arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.write");
    
    return node;
}

static ASTNode* ioSeekStatement() {
    ASTNode* node = newNode(NODE_FILE_SEEK);
    
    consume(TK_LPAREN, "Expected '(' after io.seek");
    node->left = expression(); // fd
    consume(TK_COMMA, "Expected ',' after fd");
    node->right = expression(); // offset
    
    if (match(TK_COMMA)) {
        node->third = expression(); // whence
    }
    
    consume(TK_RPAREN, "Expected ')' after io.seek arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.seek");
    
    return node;
}

static ASTNode* ioTellStatement() {
    ASTNode* node = newNode(NODE_FILE_TELL);
    
    consume(TK_LPAREN, "Expected '(' after io.tell");
    node->left = expression(); // fd
    
    if (match(TK_COMMA)) {
        node->right = expression(); // variable pour résultat
    }
    
    consume(TK_RPAREN, "Expected ')' after io.tell arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.tell");
    
    return node;
}

static ASTNode* ioExistsStatement() {
    ASTNode* node = newNode(NODE_PATH_EXISTS);
    
    consume(TK_LPAREN, "Expected '(' after io.exists");
    node->left = expression(); // path
    
    if (match(TK_COMMA)) {
        node->right = expression(); // variable pour résultat
    }
    
    consume(TK_RPAREN, "Expected ')' after io.exists arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.exists");
    
    return node;
}

static ASTNode* ioIsfileStatement() {
    ASTNode* node = newNode(NODE_PATH_ISFILE);
    
    consume(TK_LPAREN, "Expected '(' after io.isfile");
    node->left = expression(); // path
    
    if (match(TK_COMMA)) {
        node->right = expression(); // variable pour résultat
    }
    
    consume(TK_RPAREN, "Expected ')' after io.isfile arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.isfile");
    
    return node;
}

static ASTNode* ioIsdirStatement() {
    ASTNode* node = newNode(NODE_PATH_ISDIR);
    
    consume(TK_LPAREN, "Expected '(' after io.isdir");
    node->left = expression(); // path
    
    if (match(TK_COMMA)) {
        node->right = expression(); // variable pour résultat
    }
    
    consume(TK_RPAREN, "Expected ')' after io.isdir arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.isdir");
    
    return node;
}

static ASTNode* ioMkdirStatement() {
    ASTNode* node = newNode(NODE_DIR_CREATE);
    
    consume(TK_LPAREN, "Expected '(' after io.mkdir");
    node->left = expression(); // path
    
    if (match(TK_COMMA)) {
        node->right = expression(); // mode
    }
    
    consume(TK_RPAREN, "Expected ')' after io.mkdir arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.mkdir");
    
    return node;
}

static ASTNode* ioListdirStatement() {
    ASTNode* node = newNode(NODE_DIR_LIST);
    
    consume(TK_LPAREN, "Expected '(' after io.listdir");
    node->left = expression(); // path
    
    // Optionnel : deuxième paramètre pour les options
    if (match(TK_COMMA)) {
        node->right = expression(); // options (-a, -l, etc.)
    }
    
    consume(TK_RPAREN, "Expected ')' after io.listdir arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.listdir");
    
    return node;
}
static ASTNode* ioFlushStatement() {
    ASTNode* node = newNode(NODE_FILE_FLUSH);
    
    consume(TK_LPAREN, "Expected '(' after io.flush");
    node->left = expression(); // fd
    
    consume(TK_RPAREN, "Expected ')' after io.flush arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.flush");
    
    return node;
}
static ASTNode* ioCopyStatement() {
    ASTNode* node = newNode(NODE_FILE_COPY);
    
    consume(TK_LPAREN, "Expected '(' after io.copy");
    node->left = expression(); // source
    consume(TK_COMMA, "Expected ',' after source");
    node->right = expression(); // destination
    
    consume(TK_RPAREN, "Expected ')' after io.copy arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.copy");
    
    return node;
}
static ASTNode* ioRemoveStatement() {
    ASTNode* node = newNode(NODE_FILE_REMOVE);
    
    consume(TK_LPAREN, "Expected '(' after io.remove");
    node->left = expression(); // filename
    
    consume(TK_RPAREN, "Expected ')' after io.remove arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.remove");
    
    return node;
}
static ASTNode* ioRenameStatement() {
    ASTNode* node = newNode(NODE_FILE_RENAME);
    
    consume(TK_LPAREN, "Expected '(' after io.rename");
    node->left = expression(); // old name
    consume(TK_COMMA, "Expected ',' after old name");
    node->right = expression(); // new name
    
    consume(TK_RPAREN, "Expected ')' after io.rename arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.rename");
    
    return node;
}
static ASTNode* ioRmdirStatement() {
    ASTNode* node = newNode(NODE_DIR_REMOVE);
    
    consume(TK_LPAREN, "Expected '(' after io.rmdir");
    node->left = expression(); // dirname
    
    consume(TK_RPAREN, "Expected ')' after io.rmdir arguments");
    consume(TK_SEMICOLON, "Expected ';' after io.rmdir");
    
    return node;
}







// Print statement
static ASTNode* printStatement() {
    ASTNode* node = newNode(NODE_PRINT);
    
    consume(TK_LPAREN, "Expected '(' after 'print'");
    
    if (!check(TK_RPAREN)) {
        node->left = expression();
        
        while (match(TK_COMMA)) {
            ASTNode* next_arg = expression();
            if (node->left) {
                ASTNode* current = node->left;
                while (current->right) current = current->right;
                current->right = next_arg;
            }
        }
    }
    
    consume(TK_RPAREN, "Expected ')' after print arguments");
    consume(TK_SEMICOLON, "Expected ';' after print statement");
    
    return node;
}

// Weld statement (input)
static ASTNode* weldStatement() {
    ASTNode* node = newNode(NODE_WELD);
    
    consume(TK_LPAREN, "Expected '(' after 'weld'");
    
    if (!check(TK_RPAREN)) {
        node->left = expression();
    }
    
    consume(TK_RPAREN, "Expected ')' after weld arguments");
    consume(TK_SEMICOLON, "Expected ';' after weld statement");
    
    return node;
}

// Read statement
static ASTNode* readStatement() {
    ASTNode* node = newNode(NODE_READ);
    
    consume(TK_LPAREN, "Expected '(' after 'read'");
    node->left = expression();
    consume(TK_RPAREN, "Expected ')' after read arguments");
    consume(TK_SEMICOLON, "Expected ';' after read statement");
    
    return node;
}

// Write statement
static ASTNode* writeStatement() {
    ASTNode* node = newNode(NODE_WRITE);
    
    consume(TK_LPAREN, "Expected '(' after 'write'");
    node->left = expression();
    
    if (match(TK_COMMA)) {
        node->right = expression();
    }
    
    consume(TK_RPAREN, "Expected ')' after write arguments");
    consume(TK_SEMICOLON, "Expected ';' after write statement");
    
    return node;
}

// Assert statement
static ASTNode* assertStatement() {
    ASTNode* node = newNode(NODE_ASSERT);
    
    consume(TK_LPAREN, "Expected '(' after 'assert'");
    node->left = expression();
    
    if (match(TK_COMMA)) {
        node->right = expression();
    }
    
    consume(TK_RPAREN, "Expected ')' after assert arguments");
    consume(TK_SEMICOLON, "Expected ';' after assert statement");
    
    return node;
}

// Pass statement
static ASTNode* passStatement() {
    consume(TK_SEMICOLON, "Expected ';' after pass");
    return newNode(NODE_PASS);
}

// With statement
static ASTNode* withStatement() {
    ASTNode* node = newNode(NODE_WITH);
    
    consume(TK_LPAREN, "Expected '(' after 'with'");
    node->left = expression();
    consume(TK_RPAREN, "Expected ')' after with expression");
    
    node->right = statement();
    
    return node;
}

// Learn statement
static ASTNode* learnStatement() {
    ASTNode* node = newNode(NODE_LEARN);
    
    if (!match(TK_IDENT)) {
        error("Expected variable name after 'learn'");
        return NULL;
    }
    
    node->data.name = str_copy(previous.value.str_val);
    
    if (match(TK_ASSIGN)) {
        node->left = expression();
    }
    
    consume(TK_SEMICOLON, "Expected ';' after learn statement");
    
    return node;
}

// Lock statement
static ASTNode* lockStatement() {
    ASTNode* node = newNode(NODE_LOCK);
    
    consume(TK_LPAREN, "Expected '(' after 'lock'");
    node->left = expression();
    consume(TK_RPAREN, "Expected ')' after lock expression");
    
    node->right = statement();
    
    return node;
}

// Append statement
static ASTNode* appendStatement() {
    ASTNode* node = newNode(NODE_APPEND);
    
    consume(TK_LPAREN, "Expected '(' after 'append'");
    node->data.append_op.list = expression();
    consume(TK_COMMA, "Expected ',' after list");
    node->data.append_op.value = expression();
    consume(TK_RPAREN, "Expected ')' after append arguments");
    consume(TK_SEMICOLON, "Expected ';' after append statement");
    
    return node;
}

// Push statement
static ASTNode* pushStatement() {
    ASTNode* node = newNode(NODE_PUSH);
    
    consume(TK_LPAREN, "Expected '(' after 'push'");
    node->data.collection_op.collection = expression();
    consume(TK_COMMA, "Expected ',' after collection");
    node->data.collection_op.value = expression();
    consume(TK_RPAREN, "Expected ')' after push arguments");
    consume(TK_SEMICOLON, "Expected ';' after push statement");
    
    return node;
}

// Pop statement
static ASTNode* popStatement() {
    ASTNode* node = newNode(NODE_POP);
    
    consume(TK_LPAREN, "Expected '(' after 'pop'");
    node->data.collection_op.collection = expression();
    consume(TK_RPAREN, "Expected ')' after pop arguments");
    consume(TK_SEMICOLON, "Expected ';' after pop statement");
    
    return node;
}

// If statement
static ASTNode* ifStatement() {
    ASTNode* node = newNode(NODE_IF);
    
    consume(TK_LPAREN, "Expected '(' after 'if'");
    node->left = expression();
    consume(TK_RPAREN, "Expected ')' after if condition");
    
    node->right = statement();
    
    if (match(TK_ELSE)) {
        node->third = statement();
    }
    
    return node;
}

// While statement
static ASTNode* whileStatement() {
    ASTNode* node = newNode(NODE_WHILE);
    
    consume(TK_LPAREN, "Expected '(' after 'while'");
    node->left = expression();
    consume(TK_RPAREN, "Expected ')' after while condition");
    
    node->right = statement();
    
    return node;
}

// Do-while statement
static ASTNode* doWhileStatement() {
    ASTNode* node = newNode(NODE_WHILE); // Reuse while node
    
    consume(TK_DO, "Expected 'do'");
    node->right = statement();
    
    consume(TK_WHILE, "Expected 'while' after do block");
    consume(TK_LPAREN, "Expected '(' after 'while'");
    node->left = expression();
    consume(TK_RPAREN, "Expected ')' after while condition");
    consume(TK_SEMICOLON, "Expected ';' after do-while statement");
    
    return node;
}

// For statement
static ASTNode* forStatement() {
    ASTNode* node = newNode(NODE_FOR);
    
    consume(TK_LPAREN, "Expected '(' after 'for'");
    
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
    consume(TK_SEMICOLON, "Expected ';' after loop condition");
    
    // Increment
    if (!check(TK_RPAREN)) {
        node->data.loop.update = expression();
    }
    consume(TK_RPAREN, "Expected ')' after for clauses");
    
    node->data.loop.body = statement();
    
    return node;
}

// For-in statement
static ASTNode* forInStatement() {
    ASTNode* node = newNode(NODE_FOR_IN);
    
    consume(TK_LPAREN, "Expected '(' after 'for'");
    
    consume(TK_IDENT, "Expected variable name in for-in loop");
    node->data.for_in.var_name = str_copy(previous.value.str_val);
    
    consume(TK_IN, "Expected 'in' in for-in loop");
    node->data.for_in.iterable = expression();
    
    consume(TK_RPAREN, "Expected ')' after for-in expression");
    node->data.for_in.body = statement();
    
    return node;
}

// Switch statement
static ASTNode* switchStatement() {
    ASTNode* node = newNode(NODE_SWITCH);
    
    consume(TK_LPAREN, "Expected '(' after 'switch'");
    node->data.switch_stmt.expr = expression();
    consume(TK_RPAREN, "Expected ')' after switch expression");
    
    consume(TK_LBRACE, "Expected '{' after switch");
    
    ASTNode* first_case = NULL;
    ASTNode* current_case = NULL;
    
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        if (match(TK_CASE)) {
            ASTNode* case_node = newNode(NODE_CASE);
            case_node->data.case_stmt.value = expression();
            consume(TK_COLON, "Expected ':' after case value");
            
            // Parse case body
            ASTNode* case_body = NULL;
            ASTNode* current_stmt = NULL;
            
            while (!check(TK_CASE) && !check(TK_DEFAULT) && !check(TK_RBRACE)) {
                ASTNode* stmt = statement();
                if (stmt) {
                    if (!case_body) {
                        case_body = stmt;
                        current_stmt = stmt;
                    } else {
                        current_stmt->right = stmt;
                        current_stmt = stmt;
                    }
                }
            }
            
            case_node->data.case_stmt.body = case_body;
            
            if (!first_case) {
                first_case = case_node;
                current_case = case_node;
            } else {
                current_case->right = case_node;
                current_case = case_node;
            }
        } else if (match(TK_DEFAULT)) {
            consume(TK_COLON, "Expected ':' after default");
            
            ASTNode* default_body = NULL;
            ASTNode* current_stmt = NULL;
            
            while (!check(TK_RBRACE)) {
                ASTNode* stmt = statement();
                if (stmt) {
                    if (!default_body) {
                        default_body = stmt;
                        current_stmt = stmt;
                    } else {
                        current_stmt->right = stmt;
                        current_stmt = stmt;
                    }
                }
            }
            
            node->data.switch_stmt.default_case = default_body;
        } else {
            errorAtCurrent("Expected 'case' or 'default'");
            break;
        }
    }
    
    consume(TK_RBRACE, "Expected '}' after switch");
    node->data.switch_stmt.cases = first_case;
    
    return node;
}

// Return statement
static ASTNode* returnStatement() {
    ASTNode* node = newNode(NODE_RETURN);
    
    if (!check(TK_SEMICOLON)) {
        node->left = expression();
    }
    
    consume(TK_SEMICOLON, "Expected ';' after return value");
    
    return node;
}

// Yield statement
static ASTNode* yieldStatement() {
    ASTNode* node = newNode(NODE_YIELD);
    
    if (!check(TK_SEMICOLON)) {
        node->left = expression();
    }
    
    consume(TK_SEMICOLON, "Expected ';' after yield value");
    
    return node;
}

// Break statement
static ASTNode* breakStatement() {
    ASTNode* node = newNode(NODE_BREAK);
    
    if (match(TK_IDENT)) {
        node->data.name = str_copy(previous.value.str_val);
    }
    
    consume(TK_SEMICOLON, "Expected ';' after break");
    
    return node;
}

// Continue statement
static ASTNode* continueStatement() {
    ASTNode* node = newNode(NODE_CONTINUE);
    
    if (match(TK_IDENT)) {
        node->data.name = str_copy(previous.value.str_val);
    }
    
    consume(TK_SEMICOLON, "Expected ';' after continue");
    
    return node;
}

// Throw statement
static ASTNode* throwStatement() {
    ASTNode* node = newNode(NODE_THROW);
    
    node->left = expression();
    consume(TK_SEMICOLON, "Expected ';' after throw");
    
    return node;
}

// Try statement
static ASTNode* tryStatement() {
    ASTNode* node = newNode(NODE_TRY);
    
    node->data.try_catch.try_block = block();
    
    if (match(TK_CATCH)) {
        consume(TK_LPAREN, "Expected '(' after 'catch'");
        
        if (match(TK_IDENT)) {
            node->data.try_catch.error_var = str_copy(previous.value.str_val);
        }
        
        consume(TK_RPAREN, "Expected ')' after catch parameter");
        node->data.try_catch.catch_block = block();
    }
    
    if (match(TK_FINALLY)) {
        node->data.try_catch.finally_block = block();
    }
    
    return node;
}

// Expression statement
static ASTNode* expressionStatement() {
    ASTNode* expr = expression();
    consume(TK_SEMICOLON, "Expected ';' after expression");
    return expr;
}

// Block statement
static ASTNode* block() {
    ASTNode* node = newNode(NODE_BLOCK);
    ASTNode* current = NULL;
    
    int prev_scope = scope_level;
    scope_level++;
    
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
    
    consume(TK_RBRACE, "Expected '}' after block");
    
    scope_level = prev_scope;
    
    return node;
}

// ======================================================
// [SECTION] FUNCTION DECLARATION - COMPLETE
// ======================================================
static ASTNode* functionDeclaration(bool is_exported) {
    Token func_token = previous;
    /* SUPPORT IO
    if (match(TK_IO_OPEN)) return ioOpenStatement();
    if (match(TK_IO_CLOSE)) return ioCloseStatement();
    if (match(TK_IO_READ)) return ioReadStatement();
    if (match(TK_IO_WRITE)) return ioWriteStatement();
    if (!match(TK_IDENT)) {
        errorAtCurrent("Expected function name after 'func'");
        return NULL;
    } */
    
    char* func_name = str_copy(previous.value.str_val);
    
    // Parse type parameters (generics)
    ASTNode* type_params = NULL;
    if (match(TK_LT)) {
        type_params = newNode(NODE_TYPE);
        
        if (match(TK_IDENT)) {
            ASTNode* first_param = newIdentNode(previous.value.str_val);
            ASTNode* current_param = first_param;
            
            while (match(TK_COMMA)) {
                if (!match(TK_IDENT)) {
                    errorAtCurrent("Expected type parameter name after comma");
                    break;
                }
                ASTNode* param = newIdentNode(previous.value.str_val);
                if (current_param) {
                    current_param->right = param;
                    current_param = param;
                }
            }
            
            type_params->left = first_param;
        }
        
        consume(TK_GT, "Expected '>' after type parameters");
    }
    
    // Parse parameters
    consume(TK_LPAREN, "Expected '(' after function name");
    
    ASTNode* first_param = NULL;
    ASTNode* current_param = NULL;
    int param_count = 0;
    
    if (!check(TK_RPAREN)) {
        if (match(TK_IDENT)) {
            first_param = newIdentNode(previous.value.str_val);
            current_param = first_param;
            param_count = 1;
            
            // Optional type annotation
            if (match(TK_COLON)) {
                // Skip type for now
                while (!check(TK_COMMA) && !check(TK_RPAREN) && !check(TK_ASSIGN)) {
                    advance();
                }
            }
            
            // Optional default value
            if (match(TK_ASSIGN)) {
                // Skip default value for now
                ASTNode* default_val = expression();
                if (default_val) {
                    // Store in param node if needed
                }
            }
            
            while (match(TK_COMMA)) {
                if (!match(TK_IDENT)) {
                    errorAtCurrent("Expected parameter name after comma");
                    break;
                }
                
                ASTNode* param = newIdentNode(previous.value.str_val);
                if (current_param) {
                    current_param->right = param;
                    current_param = param;
                }
                param_count++;
                
                // Optional type annotation
                if (match(TK_COLON)) {
                    // Skip type
                    while (!check(TK_COMMA) && !check(TK_RPAREN) && !check(TK_ASSIGN)) {
                        advance();
                    }
                }
                
                // Optional default value
                if (match(TK_ASSIGN)) {
                    // Skip default value
                    ASTNode* default_val = expression();
                }
            }
        }
    }
    
    consume(TK_RPAREN, "Expected ')' after parameters");
    
    // Optional return type
    if (match(TK_COLON)) {
        // Skip return type for now
        while (!check(TK_LBRACE) && !check(TK_EOF)) {
            advance();
        }
    }
    
    // Parse function body
    consume(TK_LBRACE, "Expected '{' before function body");
    
    ASTNode* node = newNode(NODE_FUNC);
    node->data.name = func_name;
    node->line = func_token.line;
    node->column = func_token.column;
    
    if (type_params) {
        node->third = type_params; // Store type params in third field
    }
    
    if (first_param) {
        node->left = first_param;
    }
    
    // Parse body
    int prev_scope = scope_level;
    scope_level++;
    
    node->right = block();
    
    scope_level = prev_scope;
    
    printf("%s[PARSER]%s Function '%s' declared with %d parameters\n", 
           COLOR_GREEN, COLOR_RESET, func_name, param_count);
    
    return node;
}

// Variable declaration
static ASTNode* variableDeclaration() {
    TokenKind declType = previous.kind;
    
    if (!match(TK_IDENT)) {
        errorAtCurrent("Expected variable name");
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
        case TK_GLOBAL: node = newNode(NODE_GLOBAL_DECL); break;
        default: node = newNode(NODE_VAR_DECL); break;
    }
    
    if (!node) {
        free(varName);
        return NULL;
    }
    
    node->data.name = varName;
    
    // Optional type annotation
    if (match(TK_COLON)) {
        // Skip type for now
        while (!check(TK_ASSIGN) && !check(TK_SEMICOLON)) {
            advance();
        }
    }
    
    if (match(TK_ASSIGN)) {
        node->left = expression();
    }
    
    consume(TK_SEMICOLON, "Expected ';' after variable declaration");
    
    return node;
}

// ======================================================
// [SECTION] CLASS AND TYPE DECLARATIONS - COMPLETE
// ======================================================
static ASTNode* classDeclaration() {
    Token class_token = previous;
    
    if (!match(TK_IDENT)) {
        errorAtCurrent("Expected class name");
        return NULL;
    }
    
    char* class_name = str_copy(previous.value.str_val);
    ASTNode* node = newNode(NODE_CLASS);
    node->data.class_def.name = class_name;
    
    // Type parameters (generics)
    if (match(TK_LT)) {
        // Skip type parameters for now
        while (!match(TK_GT) && !check(TK_EOF)) {
            advance();
        }
    }
    
    // Inheritance
    if (match(TK_COLON)) {
        if (!match(TK_IDENT)) {
            errorAtCurrent("Expected parent class name");
            free(class_name);
            free(node);
            return NULL;
        }
        node->data.class_def.parent = newIdentNode(previous.value.str_val);
    }
    
    consume(TK_LBRACE, "Expected '{' before class body");
    
    // Parse class members
    ASTNode* first_member = NULL;
    ASTNode* current_member = NULL;
    
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        ASTNode* member = NULL;
        
        // Access modifiers
        if (match(TK_PUBLIC) || match(TK_PRIVATE) || match(TK_PROTECTED) || match(TK_STATIC)) {
            TokenKind modifier = previous.kind;
            
            if (match(TK_VAR) || match(TK_LET) || match(TK_CONST)) {
                member = variableDeclaration();
                if (member) {
                    member->op_type = modifier; // Store modifier in op_type
                }
            } else if (match(TK_FUNC)) {
                member = functionDeclaration(false);
                if (member) {
                    member->op_type = modifier;
                }
            } else {
                errorAtCurrent("Expected member declaration after access modifier");
                advance();
                continue;
            }
        } else if (match(TK_VAR) || match(TK_LET) || match(TK_CONST)) {
            member = variableDeclaration();
        } else if (match(TK_FUNC)) {
            member = functionDeclaration(false);
        } else if (match(TK_CLASS)) {
            member = classDeclaration();
        } else if (match(TK_STRUCT)) {
            member = structDeclaration();
        } else {
            errorAtCurrent("Expected class member");
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
    
    consume(TK_RBRACE, "Expected '}' after class body");
    
    node->data.class_def.members = first_member;
    
    printf("%s[PARSER]%s Class '%s' defined\n", 
           COLOR_MAGENTA, COLOR_RESET, class_name);
    
    return node;
}

static ASTNode* structDeclaration() {
    // Similar to class for now
    return classDeclaration();
}

static ASTNode* enumDeclaration() {
    Token enum_token = previous;
    
    if (!match(TK_IDENT)) {
        errorAtCurrent("Expected enum name");
        return NULL;
    }
    
    char* enum_name = str_copy(previous.value.str_val);
    ASTNode* node = newNode(NODE_ENUM);
    node->data.name = enum_name;
    
    consume(TK_LBRACE, "Expected '{' before enum body");
    
    // Parse enum variants
    ASTNode* first_variant = NULL;
    ASTNode* current_variant = NULL;
    
    if (!check(TK_RBRACE)) {
        do {
            if (!match(TK_IDENT)) {
                errorAtCurrent("Expected enum variant name");
                break;
            }
            
            ASTNode* variant = newIdentNode(previous.value.str_val);
            
            // Optional value assignment
            if (match(TK_ASSIGN)) {
                ASTNode* value = expression();
                if (value) {
                    variant->left = value;
                }
            }
            
            if (!first_variant) {
                first_variant = variant;
                current_variant = variant;
            } else {
                current_variant->right = variant;
                current_variant = variant;
            }
            
        } while (match(TK_COMMA));
    }
    
    consume(TK_RBRACE, "Expected '}' after enum");
    consume(TK_SEMICOLON, "Expected ';' after enum declaration");
    
    node->left = first_variant;
    
    printf("%s[PARSER]%s Enum '%s' defined\n", 
           COLOR_MAGENTA, COLOR_RESET, enum_name);
    
    return node;
}

static ASTNode* interfaceDeclaration() {
    // Similar to class for now
    return classDeclaration();
}

static ASTNode* typedefDeclaration() {
    Token typedef_token = previous;
    
    if (!match(TK_IDENT)) {
        errorAtCurrent("Expected type name after typedef");
        return NULL;
    }
    
    char* type_name = str_copy(previous.value.str_val);
    ASTNode* node = newNode(NODE_TYPEDEF);
    node->data.name = type_name;
    
    consume(TK_ASSIGN, "Expected '=' in typedef");
    
    // Parse type definition (simplified)
    while (!check(TK_SEMICOLON) && !check(TK_EOF)) {
        advance();
    }
    
    consume(TK_SEMICOLON, "Expected ';' after typedef");
    
    printf("%s[PARSER]%s Typedef '%s' defined\n", 
           COLOR_CYAN, COLOR_RESET, type_name);
    
    return node;
}

static ASTNode* namespaceDeclaration() {
    if (!match(TK_IDENT)) {
        errorAtCurrent("Expected namespace name");
        return NULL;
    }
    
    char* ns_name = str_copy(previous.value.str_val);
    ASTNode* node = newNode(NODE_NAMESPACE);
    node->data.name = ns_name;
    
    consume(TK_LBRACE, "Expected '{' before namespace body");
    
    // Parse namespace body
    ASTNode* first_decl = NULL;
    ASTNode* current_decl = NULL;
    
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        ASTNode* decl = declaration();
        if (decl) {
            if (!first_decl) {
                first_decl = decl;
                current_decl = decl;
            } else {
                current_decl->right = decl;
                current_decl = decl;
            }
        }
    }
    
    consume(TK_RBRACE, "Expected '}' after namespace");
    
    node->left = first_decl;
    
    printf("%s[PARSER]%s Namespace '%s' defined\n", 
           COLOR_CYAN, COLOR_RESET, ns_name);
    
    return node;
}

// ======================================================
// [SECTION] MODULE DECLARATIONS
// ======================================================
static ASTNode* importStatement() {
    Token import_token = previous;
    
    if (!match(TK_STRING)) {
        errorAtCurrent("Expected module name after import");
        return NULL;
    }
    
    char* module_name = str_copy(previous.value.str_val);
    ASTNode* node = newNode(NODE_IMPORT);
    
    node->data.imports.modules = malloc(sizeof(char*));
    node->data.imports.modules[0] = module_name;
    node->data.imports.module_count = 1;
    
    // Multiple imports
    while (match(TK_COMMA)) {
        if (!match(TK_STRING)) {
            errorAtCurrent("Expected module name after comma");
            break;
        }
        
        node->data.imports.module_count++;
        node->data.imports.modules = realloc(node->data.imports.modules, 
                                           node->data.imports.module_count * sizeof(char*));
        node->data.imports.modules[node->data.imports.module_count - 1] = 
            str_copy(previous.value.str_val);
    }
    
    // Optional 'from' clause
    if (match(TK_FROM)) {
        if (!match(TK_STRING)) {
            errorAtCurrent("Expected package name after 'from'");
            // Cleanup
            for (int i = 0; i < node->data.imports.module_count; i++) {
                free(node->data.imports.modules[i]);
            }
            free(node->data.imports.modules);
            free(node);
            return NULL;
        }
        node->data.imports.from_module = str_copy(previous.value.str_val);
    }
    
    consume(TK_SEMICOLON, "Expected ';' after import statement");
    
    return node;
}

static ASTNode* exportStatement() {
    Token export_token = previous;
    
    if (!match(TK_STRING)) {
        errorAtCurrent("Expected symbol name after export");
        return NULL;
    }
    
    char* symbol = str_copy(previous.value.str_val);
    ASTNode* node = newNode(NODE_EXPORT);
    node->data.export.symbol = symbol;
    
    if (match(TK_AS)) {
        if (!match(TK_STRING)) {
            errorAtCurrent("Expected alias name after 'as'");
            free(symbol);
            free(node);
            return NULL;
        }
        node->data.export.alias = str_copy(previous.value.str_val);
    } else {
        node->data.export.alias = str_copy(symbol);
    }
    
    consume(TK_SEMICOLON, "Expected ';' after export statement");
    
    return node;
}

// ======================================================
// [SECTION] SPECIAL DECLARATIONS
// ======================================================
static ASTNode* mainDeclaration() {
    consume(TK_LPAREN, "Expected '(' after main");
    
    // Parse parameters (usually empty for main)
    if (!match(TK_RPAREN)) {
        // Could parse argc/argv here
        while (!match(TK_RPAREN) && !check(TK_EOF)) {
            advance();
        }
    }
    
    consume(TK_LBRACE, "Expected '{' before main body");
    
    ASTNode* node = newNode(NODE_MAIN);
    node->left = block();
    
    printf("%s[PARSER]%s Main function parsed\n", COLOR_BLUE, COLOR_RESET);
    
    return node;
}

static ASTNode* dbvarStatement() {
    consume(TK_SEMICOLON, "Expected ';' after dbvar");
    return newNode(NODE_DBVAR);
}

// ======================================================
// [SECTION] DECLARATION PARSING - COMPLETE
// ======================================================
static ASTNode* declaration() {
    if (match(TK_IMPORT)) return importStatement();
    if (match(TK_EXPORT)) return exportStatement();
    if (match(TK_MAIN)) return mainDeclaration();
    if (match(TK_DBVAR)) return dbvarStatement();
    if (match(TK_TYPEDEF)) return typedefDeclaration();
    if (match(TK_NAMESPACE)) return namespaceDeclaration();
    if (match(TK_CLASS)) return classDeclaration();
    if (match(TK_STRUCT)) return structDeclaration();
    if (match(TK_ENUM)) return enumDeclaration();
    if (match(TK_INTERFACE)) return interfaceDeclaration();
    if (match(TK_TYPELOCK)) {
        // Skip typelock for now
        warningAt(previous, "typelock not implemented");
        while (!check(TK_SEMICOLON) && !check(TK_EOF)) advance();
        if (match(TK_SEMICOLON)) {
            return newNode(NODE_EMPTY);
        }
        return NULL;
    }
    
    // Check for async functions
    bool is_async = false;
    if (match(TK_ASYNC)) {
        is_async = true;
    }
    
    if (match(TK_FUNC)) {
        ASTNode* func = functionDeclaration(false);
        if (func && is_async) {
            ASTNode* async_node = newNode(NODE_ASYNC);
            async_node->left = func;
            return async_node;
        }
        return func;
    }
    
    // Variable declarations
    if (match(TK_VAR) || match(TK_LET) || match(TK_CONST) ||
        match(TK_NET) || match(TK_CLOG) || match(TK_DOS) || match(TK_SEL) ||
        match(TK_GLOBAL)) {
        return variableDeclaration();
    }
    
    return statement();
}

static ASTNode* statement() {
    if (match(TK_PRINT)) return printStatement();
    if (match(TK_PRINT_DB)) {
        warningAt(previous, "printdb not implemented, using print");
        return printStatement();
    }
    // Après les autres match() statements
    static ASTNode* statement() {
    if (match(TK_IO_OPEN)) return ioOpenStatement();
    if (match(TK_IO_CLOSE)) return ioCloseStatement();
    if (match(TK_IO_READ)) return ioReadStatement();
    if (match(TK_IO_WRITE)) return ioWriteStatement();
    if (match(TK_IO_SEEK)) return ioSeekStatement();
    if (match(TK_IO_TELL)) return ioTellStatement();
    if (match(TK_IO_FLUSH)) return ioFlushStatement();
    if (match(TK_IO_EXISTS)) return ioExistsStatement();
    if (match(TK_IO_ISFILE)) return ioIsfileStatement();
    if (match(TK_IO_ISDIR)) return ioIsdirStatement();
    if (match(TK_IO_MKDIR)) return ioMkdirStatement();
    if (match(TK_IO_RMDIR)) return ioRmdirStatement();
    if (match(TK_IO_LISTDIR)) return ioListdirStatement();
    if (match(TK_IO_REMOVE)) return ioRemoveStatement();
    if (match(TK_IO_RENAME)) return ioRenameStatement();
    if (match(TK_IO_COPY)) return ioCopyStatement();
    if (match(TK_WELD)) return weldStatement();
    if (match(TK_READ)) return readStatement();
    if (match(TK_WRITE)) return writeStatement();
    if (match(TK_ASSERT)) return assertStatement();
    if (match(TK_PASS)) return passStatement();
    if (match(TK_WITH)) return withStatement();
    if (match(TK_LEARN)) return learnStatement();
    if (match(TK_LOCK)) return lockStatement();
    if (match(TK_APPEND)) return appendStatement();
    if (match(TK_PUSH)) return pushStatement();
    if (match(TK_POP)) return popStatement();
    if (match(TK_IF)) return ifStatement();
    if (match(TK_WHILE)) return whileStatement();
    if (match(TK_DO)) return doWhileStatement();
    if (match(TK_FOR)) {
        // Check if it's for-in
        Token saved = current;
        bool is_for_in = false;
        
        // Look ahead
        advance(); // Skip 'for'
        if (match(TK_IDENT) && match(TK_IN)) {
            is_for_in = true;
        }
        
        // Restore
        current = saved;
        
        if (is_for_in) {
            return forInStatement();
        } else {
            return forStatement();
        }
    }
    if (match(TK_SWITCH)) return switchStatement();
    if (match(TK_RETURN)) return returnStatement();
    if (match(TK_YIELD)) return yieldStatement();
    if (match(TK_BREAK)) return breakStatement();
    if (match(TK_CONTINUE)) return continueStatement();
    if (match(TK_THROW)) return throwStatement();
    if (match(TK_TRY)) return tryStatement();
    if (match(TK_LBRACE)) return block();
    
    return expressionStatement();
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
    warningCount = 0;
    scope_level = 0;
    
    printf("%s[PARSER]%s Starting parse...\n", COLOR_CYAN, COLOR_RESET);
    
    int capacity = 100;
    ASTNode** nodes = malloc(capacity * sizeof(ASTNode*));
    if (!nodes) {
        fprintf(stderr, "%s[PARSER FATAL]%s Memory allocation failed\n", COLOR_RED, COLOR_RESET);
        return NULL;
    }
    
    *count = 0;
    
    while (current.kind != TK_EOF) {
        if (*count >= capacity) {
            capacity *= 2;
            ASTNode** new_nodes = realloc(nodes, capacity * sizeof(ASTNode*));
            if (!new_nodes) {
                fprintf(stderr, "%s[PARSER FATAL]%s Memory reallocation failed\n", COLOR_RED, COLOR_RESET);
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
    
    printf("%s[PARSER]%s Parse complete. %d declarations parsed.\n", 
           COLOR_CYAN, COLOR_RESET, *count);
    printf("%s[PARSER]%s Errors: %d, Warnings: %d\n", 
           errorCount > 0 ? COLOR_RED : COLOR_GREEN, COLOR_RESET, errorCount, warningCount);
    
    if (errorCount > 0) {
        printf("%s[PARSER]%s Parse completed with errors\n", COLOR_RED, COLOR_RESET);
    } else {
        printf("%s[PARSER]%s Parse successful\n", COLOR_GREEN, COLOR_RESET);
    }
    
    return nodes;
}

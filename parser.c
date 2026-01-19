#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "common.h"

extern Token scan_token();
extern Token peek_token();
extern Token previous_token();
extern bool is_at_end_wrapper();
extern void init_lexer(const char* source);

// ======================================================
// [SECTION] PARSER STATE
// ======================================================
static Token current;
static Token previous;
static bool had_error = false;
static bool panic_mode = false;
static Error* parse_error = NULL;

// ======================================================
// [SECTION] PARSER UTILITIES
// ======================================================
static void advance() {
    previous = current;
    
    if (has_peek) {
        current = peek_token;
        has_peek = false;
    } else {
        current = scan_token();
    }
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

static bool check_next(TokenKind kind) {
    if (is_at_end_wrapper()) return false;
    Token next = peek_token();
    return next.kind == kind;
}

static void error_at(Token token, const char* message) {
    if (panic_mode) return;
    panic_mode = true;
    
    if (parse_error) {
        set_error(parse_error, token.line, token.column, NULL, "Parser error: %s", message);
    }
    
    fprintf(stderr, RED "[PARSER ERROR]" RESET " Line %d, Col %d: %s\n", 
            token.line, token.column, message);
    had_error = true;
}

static void error(const char* message) {
    error_at(previous, message);
}

static void error_current(const char* message) {
    error_at(current, message);
}

static void synchronize() {
    panic_mode = false;
    
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
            case TK_FOREACH:
            case TK_IF:
            case TK_WHILE:
            case TK_PRINT:
            case TK_RETURN:
            case TK_IMPORT:
            case TK_EXPORT:
            case TK_TRY:
            case TK_THROW:
            case TK_ASYNC:
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
static ASTNode* new_node(NodeType type) {
    ASTNode* node = calloc(1, sizeof(ASTNode));
    if (node) {
        node->type = type;
        node->line = previous.line;
        node->column = previous.column;
        node->node_type = TYPE_NONE;
        node->is_constant = false;
        node->is_mutable = true;
        node->scope_depth = 0;
        node->module_name = NULL;
    }
    return node;
}

static ASTNode* new_int_node(int64_t value) {
    ASTNode* node = new_node(NODE_INT);
    if (node) {
        node->data.int_val = value;
        node->node_type = TYPE_INT;
    }
    return node;
}

static ASTNode* new_float_node(double value) {
    ASTNode* node = new_node(NODE_FLOAT);
    if (node) {
        node->data.float_val = value;
        node->node_type = TYPE_FLOAT;
    }
    return node;
}

static ASTNode* new_string_node(char* value) {
    ASTNode* node = new_node(NODE_STRING);
    if (node) {
        node->data.str_val = str_copy(value);
        node->node_type = TYPE_STRING;
    }
    return node;
}

static ASTNode* new_char_node(char value) {
    ASTNode* node = new_node(NODE_CHAR);
    if (node) {
        node->data.char_val = value;
        node->node_type = TYPE_CHAR;
    }
    return node;
}

static ASTNode* new_bool_node(bool value) {
    ASTNode* node = new_node(NODE_BOOL);
    if (node) {
        node->data.bool_val = value;
        node->node_type = TYPE_BOOL;
    }
    return node;
}

static ASTNode* new_ident_node(char* name) {
    ASTNode* node = new_node(NODE_IDENT);
    if (node) {
        node->data.name = str_copy(name);
        node->node_type = TYPE_NONE;
    }
    return node;
}

static ASTNode* new_null_node() {
    ASTNode* node = new_node(NODE_NULL);
    if (node) {
        node->node_type = TYPE_NULL;
    }
    return node;
}

static ASTNode* new_undefined_node() {
    ASTNode* node = new_node(NODE_UNDEFINED);
    if (node) {
        node->node_type = TYPE_UNDEFINED;
    }
    return node;
}

// ======================================================
// [SECTION] EXPRESSION PARSING
// ======================================================
static ASTNode* expression();
static ASTNode* assignment();
static ASTNode* ternary();
static ASTNode* logic_or();
static ASTNode* logic_and();
static ASTNode* equality();
static ASTNode* comparison();
static ASTNode* bitwise_or();
static ASTNode* bitwise_xor();
static ASTNode* bitwise_and();
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
    
    if (match(TK_ASSIGN) || match(TK_PLUS_ASSIGN) || match(TK_MINUS_ASSIGN) ||
        match(TK_MULT_ASSIGN) || match(TK_DIV_ASSIGN) || match(TK_MOD_ASSIGN) ||
        match(TK_AND_ASSIGN) || match(TK_OR_ASSIGN) || match(TK_XOR_ASSIGN) ||
        match(TK_SHL_ASSIGN) || match(TK_SHR_ASSIGN)) {
        
        TokenKind op = previous.kind;
        ASTNode* value = assignment();
        
        if (expr->type != NODE_IDENT && expr->type != NODE_MEMBER_ACCESS) {
            error("Invalid assignment target");
            return expr;
        }
        
        ASTNode* node = new_node(NODE_ASSIGN);
        if (node) {
            if (expr->type == NODE_IDENT) {
                node->data.name = str_copy(expr->data.name);
            } else {
                // Pour member access, on garde la structure
                node->left = expr;
            }
            node->right = value;
            node->op_type = op;
        }
        
        // Nettoyer l'ancien nœud ident si nécessaire
        if (expr->type == NODE_IDENT) {
            free(expr->data.name);
            free(expr);
        }
        
        return node;
    }
    
    return expr;
}

static ASTNode* ternary() {
    ASTNode* expr = logic_or();
    
    if (match(TK_QUESTION)) {
        ASTNode* node = new_node(NODE_IF); // Réutiliser NODE_IF pour ternary
        if (node) {
            node->left = expr; // Condition
            node->right = expression(); // Then
            
            if (!match(TK_COLON)) {
                error("Expected ':' in ternary operator");
                return node;
            }
            
            node->third = ternary(); // Else
        }
        return node;
    }
    
    return expr;
}

static ASTNode* logic_or() {
    ASTNode* expr = logic_and();
    
    while (match(TK_OR)) {
        ASTNode* node = new_node(NODE_BINARY);
        if (node) {
            node->op_type = TK_OR;
            node->left = expr;
            node->right = logic_and();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* logic_and() {
    ASTNode* expr = equality();
    
    while (match(TK_AND)) {
        ASTNode* node = new_node(NODE_BINARY);
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
    
    while (match(TK_EQ) || match(TK_NEQ)) {
        TokenKind op = previous.kind;
        ASTNode* node = new_node(NODE_BINARY);
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
    ASTNode* expr = bitwise_or();
    
    while (match(TK_GT) || match(TK_GTE) || match(TK_LT) || match(TK_LTE)) {
        TokenKind op = previous.kind;
        ASTNode* node = new_node(NODE_BINARY);
        if (node) {
            node->op_type = op;
            node->left = expr;
            node->right = bitwise_or();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* bitwise_or() {
    ASTNode* expr = bitwise_xor();
    
    while (match(TK_BIT_OR)) {
        ASTNode* node = new_node(NODE_BINARY);
        if (node) {
            node->op_type = TK_BIT_OR;
            node->left = expr;
            node->right = bitwise_xor();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* bitwise_xor() {
    ASTNode* expr = bitwise_and();
    
    while (match(TK_BIT_XOR)) {
        ASTNode* node = new_node(NODE_BINARY);
        if (node) {
            node->op_type = TK_BIT_XOR;
            node->left = expr;
            node->right = bitwise_and();
            expr = node;
        }
    }
    
    return expr;
}

static ASTNode* bitwise_and() {
    ASTNode* expr = shift();
    
    while (match(TK_BIT_AND)) {
        ASTNode* node = new_node(NODE_BINARY);
        if (node) {
            node->op_type = TK_BIT_AND;
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
        ASTNode* node = new_node(NODE_BINARY);
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
        ASTNode* node = new_node(NODE_BINARY);
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
        ASTNode* node = new_node(NODE_BINARY);
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
    if (match(TK_MINUS) || match(TK_NOT) || match(TK_BIT_NOT) || match(TK_INC) || match(TK_DEC)) {
        TokenKind op = previous.kind;
        ASTNode* node = new_node(NODE_UNARY);
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
            ASTNode* node = new_node(NODE_CALL);
            if (node) {
                node->left = expr;
                
                // Parse arguments
                ASTNode** args = NULL;
                int arg_count = 0;
                int capacity = 4;
                args = malloc(capacity * sizeof(ASTNode*));
                
                if (!check(TK_RPAREN)) {
                    do {
                        if (arg_count >= capacity) {
                            capacity *= 2;
                            args = realloc(args, capacity * sizeof(ASTNode*));
                        }
                        args[arg_count++] = expression();
                    } while (match(TK_COMMA));
                }
                
                if (!match(TK_RPAREN)) {
                    error("Expected ')' after arguments");
                }
                
                // Store arguments
                if (arg_count > 0) {
                    ASTNode* args_node = new_node(NODE_ARRAY);
                    args_node->data.array.elements = args;
                    args_node->data.array.element_count = arg_count;
                    node->right = args_node;
                } else {
                    free(args);
                }
                
                expr = node;
            }
        } else if (match(TK_PERIOD) || match(TK_SAFE_NAV)) {
            // Member access or method call
            TokenKind access_type = previous.kind;
            if (!match(TK_IDENT)) {
                error("Expected property name after '.'");
                break;
            }
            
            ASTNode* node = new_node(NODE_MEMBER_ACCESS);
            if (node) {
                node->op_type = access_type;
                node->left = expr;
                node->data.name = str_copy(previous.value.str_val);
                expr = node;
            }
        } else if (match(TK_LBRACKET)) {
            // Array/Map access
            ASTNode* node = new_node(NODE_BINARY);
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
    if (match(TK_TRUE)) return new_bool_node(true);
    if (match(TK_FALSE)) return new_bool_node(false);
    if (match(TK_NULL)) return new_null_node();
    if (match(TK_UNDEFINED)) return new_undefined_node();
    
    if (match(TK_INT)) return new_int_node(previous.value.int_val);
    if (match(TK_FLOAT)) return new_float_node(previous.value.float_val);
    if (match(TK_STRING)) return new_string_node(previous.value.str_val);
    if (match(TK_CHAR)) return new_char_node(previous.value.char_val);
    
    if (match(TK_IDENT)) return new_ident_node(previous.value.str_val);
    
    if (match(TK_THIS)) {
        ASTNode* node = new_node(NODE_THIS);
        return node;
    }
    
    if (match(TK_SUPER)) {
        ASTNode* node = new_node(NODE_SUPER);
        return node;
    }
    
    if (match(TK_AWAIT)) {
        ASTNode* node = new_node(NODE_AWAIT);
        if (node) {
            node->left = expression();
        }
        return node;
    }
    
    if (match(TK_SIZEOF) || match(TK_SIZE) || match(TK_SIZ)) {
        if (!match(TK_LPAREN)) {
            error("Expected '(' after size");
            return NULL;
        }
        
        ASTNode* node = new_node(NODE_SIZEOF);
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
    
    if (match(TK_NEW)) {
        ASTNode* node = new_node(NODE_NEW);
        if (node) {
            if (!match(TK_IDENT)) {
                error("Expected class name after 'new'");
                free(node);
                return NULL;
            }
            node->data.name = str_copy(previous.value.str_val);
            
            // Parse constructor arguments
            if (match(TK_LPAREN)) {
                ASTNode** args = NULL;
                int arg_count = 0;
                int capacity = 4;
                args = malloc(capacity * sizeof(ASTNode*));
                
                if (!check(TK_RPAREN)) {
                    do {
                        if (arg_count >= capacity) {
                            capacity *= 2;
                            args = realloc(args, capacity * sizeof(ASTNode*));
                        }
                        args[arg_count++] = expression();
                    } while (match(TK_COMMA));
                }
                
                if (!match(TK_RPAREN)) {
                    error("Expected ')' after constructor arguments");
                }
                
                if (arg_count > 0) {
                    ASTNode* args_node = new_node(NODE_ARRAY);
                    args_node->data.array.elements = args;
                    args_node->data.array.element_count = arg_count;
                    node->right = args_node;
                } else {
                    free(args);
                }
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
        ASTNode* node = new_node(NODE_ARRAY);
        if (node) {
            ASTNode** elements = NULL;
            int element_count = 0;
            int capacity = 4;
            elements = malloc(capacity * sizeof(ASTNode*));
            
            if (!check(TK_RBRACKET)) {
                do {
                    if (element_count >= capacity) {
                        capacity *= 2;
                        elements = realloc(elements, capacity * sizeof(ASTNode*));
                    }
                    elements[element_count++] = expression();
                } while (match(TK_COMMA));
            }
            
            if (!match(TK_RBRACKET)) {
                error("Expected ']' after array elements");
            }
            
            node->data.array.elements = elements;
            node->data.array.element_count = element_count;
        }
        return node;
    }
    
    if (match(TK_LBRACE)) {
        // Map/object literal
        ASTNode* node = new_node(NODE_MAP);
        if (node) {
            char** keys = NULL;
            ASTNode** values = NULL;
            int pair_count = 0;
            int capacity = 4;
            
            keys = malloc(capacity * sizeof(char*));
            values = malloc(capacity * sizeof(ASTNode*));
            
            if (!check(TK_RBRACE)) {
                do {
                    if (pair_count >= capacity) {
                        capacity *= 2;
                        keys = realloc(keys, capacity * sizeof(char*));
                        values = realloc(values, capacity * sizeof(ASTNode*));
                    }
                    
                    // Parse key
                    if (match(TK_IDENT) || match(TK_STRING)) {
                        keys[pair_count] = str_copy(previous.value.str_val);
                    } else {
                        error("Expected identifier or string as map key");
                        break;
                    }
                    
                    if (!match(TK_COLON)) {
                        error("Expected ':' after map key");
                        break;
                    }
                    
                    // Parse value
                    values[pair_count] = expression();
                    pair_count++;
                    
                } while (match(TK_COMMA));
            }
            
            if (!match(TK_RBRACE)) {
                error("Expected '}' after map elements");
            }
            
            node->data.map.keys = keys;
            node->data.map.values = values;
            node->data.map.pair_count = pair_count;
        }
        return node;
    }
    
    error("Expected expression");
    return NULL;
}

// ======================================================
// [SECTION] IMPORT STATEMENT PARSING
// ======================================================
static ASTNode* parse_import() {
    ASTNode* node = new_node(NODE_IMPORT);
    
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
    bool is_wildcard = false;
    
    // Additional imports separated by commas
    while (match(TK_COMMA)) {
        if (match(TK_MULT)) {
            // Wildcard import
            is_wildcard = true;
            char** new_imports = realloc(imports, (import_count + 1) * sizeof(char*));
            if (!new_imports) {
                for (int i = 0; i < import_count; i++) free(imports[i]);
                free(imports);
                free(node);
                return NULL;
            }
            imports = new_imports;
            imports[import_count++] = str_copy("*");
            break;
        }
        
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
    
    // Check for 'from' clause
    char* from_module = NULL;
    char* alias = NULL;
    
    if (match(TK_FROM)) {
        if (!match(TK_STRING)) {
            error("Expected package name after 'from'");
            for (int i = 0; i < import_count; i++) free(imports[i]);
            free(imports);
            free(node);
            return NULL;
        }
        from_module = str_copy(previous.value.str_val);
    }
    
    // Check for 'as' alias
    if (match(TK_AS)) {
        if (!match(TK_IDENT)) {
            error("Expected alias name after 'as'");
            for (int i = 0; i < import_count; i++) free(imports[i]);
            free(imports);
            free(from_module);
            free(node);
            return NULL;
        }
        alias = str_copy(previous.value.str_val);
    }
    
    node->data.import_export.modules = imports;
    node->data.import_export.module_count = import_count;
    node->data.import_export.from_module = from_module;
    node->data.import_export.alias = alias;
    node->data.import_export.is_wildcard = is_wildcard;
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after import statement");
    }
    
    return node;
}

// ======================================================
// [SECTION] EXPORT STATEMENT PARSING
// ======================================================
static ASTNode* parse_export() {
    ASTNode* node = new_node(NODE_EXPORT);
    
    // Syntax: export var_name;
    // Syntax: export var_name as alias;
    // Syntax: export func name() { ... }
    // Syntax: export class Name { ... }
    
    if (match(TK_VAR) || match(TK_LET) || match(TK_CONST) ||
        match(TK_NET) || match(TK_CLOG) || match(TK_DOS) || match(TK_SEL)) {
        // Export variable declaration
        ASTNode* decl = variable_declaration();
        if (decl) {
            node->left = decl;
        }
    } else if (match(TK_FUNC)) {
        // Export function declaration
        ASTNode* func = parse_function();
        if (func) {
            node->left = func;
        }
    } else if (match(TK_CLASS)) {
        // Export class declaration
        ASTNode* class_node = parse_class();
        if (class_node) {
            node->left = class_node;
        }
    } else if (match(TK_IDENT)) {
        // Export existing identifier
        node->data.import_export.modules = malloc(sizeof(char*));
        node->data.import_export.modules[0] = str_copy(previous.value.str_val);
        node->data.import_export.module_count = 1;
        
        // Optional alias
        if (match(TK_AS)) {
            if (!match(TK_IDENT)) {
                error("Expected alias name after 'as'");
                free(node->data.import_export.modules[0]);
                free(node->data.import_export.modules);
                free(node);
                return NULL;
            }
            node->data.import_export.alias = str_copy(previous.value.str_val);
        }
    } else {
        error("Expected declaration or identifier after export");
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after export statement");
    }
    
    return node;
}

// ======================================================
// [SECTION] FUNCTION PARSING
// ======================================================
static ASTNode* parse_function() {
    bool is_async = false;
    bool is_generator = false;
    
    // Check for async or generator
    if (previous.kind == TK_ASYNC) {
        is_async = true;
    } else if (previous.kind == TK_GENERATOR) {
        is_generator = true;
    }
    
    ASTNode* node = is_async ? new_node(NODE_ASYNC_FUNC) : new_node(NODE_FUNC);
    if (!node) return NULL;
    
    // Function name
    if (!match(TK_IDENT)) {
        error("Expected function name");
        free(node);
        return NULL;
    }
    node->data.name = str_copy(previous.value.str_val);
    
    // Type parameters (generics)
    if (match(TK_LANGLE)) {
        // TODO: Parse type parameters
        while (!check(TK_RANGLE) && !is_at_end_wrapper()) {
            advance();
        }
        if (!match(TK_RANGLE)) {
            error("Expected '>' after type parameters");
        }
    }
    
    // Parameters
    if (!match(TK_LPAREN)) {
        error("Expected '(' after function name");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    char** params = NULL;
    int param_count = 0;
    int capacity = 4;
    params = malloc(capacity * sizeof(char*));
    
    if (!check(TK_RPAREN)) {
        do {
            if (param_count >= capacity) {
                capacity *= 2;
                params = realloc(params, capacity * sizeof(char*));
            }
            
            if (!match(TK_IDENT)) {
                error("Expected parameter name");
                break;
            }
            params[param_count++] = str_copy(previous.value.str_val);
            
            // Optional type annotation
            if (match(TK_COLON)) {
                // Skip type for now
                if (!match(TK_IDENT)) {
                    error("Expected type name");
                }
            }
            
        } while (match(TK_COMMA));
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after parameters");
    }
    
    // Return type annotation
    if (match(TK_COLON)) {
        // Skip return type for now
        if (!match(TK_IDENT)) {
            error("Expected return type");
        }
    }
    
    // Function body
    if (!match(TK_LBRACE)) {
        error("Expected '{' before function body");
        for (int i = 0; i < param_count; i++) free(params[i]);
        free(params);
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    node->data.func.params = params;
    node->data.func.param_count = param_count;
    node->data.func.is_async = is_async;
    node->data.func.is_generator = is_generator;
    node->data.func.body = block();
    
    return node;
}

// ======================================================
// [SECTION] CLASS PARSING
// ======================================================
static ASTNode* parse_class() {
    ASTNode* node = new_node(NODE_CLASS);
    if (!node) return NULL;
    
    // Class name
    if (!match(TK_IDENT)) {
        error("Expected class name");
        free(node);
        return NULL;
    }
    node->data.class_def.class_name = str_copy(previous.value.str_val);
    
    // Inheritance
    if (match(TK_COLON)) {
        if (!match(TK_IDENT)) {
            error("Expected parent class name");
            free(node->data.class_def.class_name);
            free(node);
            return NULL;
        }
        node->data.class_def.parent_class = str_copy(previous.value.str_val);
    }
    
    // Type parameters (generics)
    if (match(TK_LANGLE)) {
        // TODO: Parse type parameters
        while (!check(TK_RANGLE) && !is_at_end_wrapper()) {
            advance();
        }
        if (!match(TK_RANGLE)) {
            error("Expected '>' after type parameters");
        }
    }
    
    // Class body
    if (!match(TK_LBRACE)) {
        error("Expected '{' before class body");
        free(node->data.class_def.class_name);
        free(node->data.class_def.parent_class);
        free(node);
        return NULL;
    }
    
    ASTNode** members = NULL;
    ASTNode** methods = NULL;
    int member_count = 0;
    int method_count = 0;
    int member_capacity = 4;
    int method_capacity = 4;
    
    members = malloc(member_capacity * sizeof(ASTNode*));
    methods = malloc(method_capacity * sizeof(ASTNode*));
    
    while (!check(TK_RBRACE) && !is_at_end_wrapper()) {
        // Parse access modifier
        bool is_public = true;
        bool is_static = false;
        
        if (match(TK_PUBLIC) || match(TK_PRIVATE) || match(TK_PROTECTED)) {
            is_public = (previous.kind == TK_PUBLIC);
        }
        
        if (match(TK_STATIC)) {
            is_static = true;
        }
        
        if (match(TK_VAR) || match(TK_LET) || match(TK_CONST)) {
            // Field declaration
            if (member_count >= member_capacity) {
                member_capacity *= 2;
                members = realloc(members, member_capacity * sizeof(ASTNode*));
            }
            
            ASTNode* field = variable_declaration();
            if (field) {
                members[member_count++] = field;
            }
        } else if (match(TK_FUNC) || match(TK_CONSTRUCTOR) || match(TK_DESTRUCTOR)) {
            // Method declaration
            if (method_count >= method_capacity) {
                method_capacity *= 2;
                methods = realloc(methods, method_capacity * sizeof(ASTNode*));
            }
            
            ASTNode* method = NULL;
            if (previous.kind == TK_CONSTRUCTOR) {
                method = parse_constructor();
            } else if (previous.kind == TK_DESTRUCTOR) {
                method = parse_destructor();
            } else {
                method = parse_function();
            }
            
            if (method) {
                methods[method_count++] = method;
            }
        } else if (match(TK_GET) || match(TK_SET)) {
            // Property getter/setter
            // TODO: Implement property parsing
            error("Properties not yet implemented");
        } else {
            error("Expected field or method declaration in class");
            break;
        }
    }
    
    if (!match(TK_RBRACE)) {
        error("Expected '}' after class body");
    }
    
    node->data.class_def.members = members;
    node->data.class_def.member_count = member_count;
    node->data.class_def.methods = methods;
    node->data.class_def.method_count = method_count;
    
    return node;
}

static ASTNode* parse_constructor() {
    ASTNode* node = new_node(NODE_CONSTRUCTOR);
    if (!node) return NULL;
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after constructor");
        free(node);
        return NULL;
    }
    
    // Parse parameters (similar to function)
    char** params = NULL;
    int param_count = 0;
    int capacity = 4;
    params = malloc(capacity * sizeof(char*));
    
    if (!check(TK_RPAREN)) {
        do {
            if (param_count >= capacity) {
                capacity *= 2;
                params = realloc(params, capacity * sizeof(char*));
            }
            
            if (!match(TK_IDENT)) {
                error("Expected parameter name");
                break;
            }
            params[param_count++] = str_copy(previous.value.str_val);
            
        } while (match(TK_COMMA));
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after parameters");
    }
    
    // Constructor body
    if (!match(TK_LBRACE)) {
        error("Expected '{' before constructor body");
        for (int i = 0; i < param_count; i++) free(params[i]);
        free(params);
        free(node);
        return NULL;
    }
    
    node->data.func.params = params;
    node->data.func.param_count = param_count;
    node->data.func.body = block();
    
    return node;
}

static ASTNode* parse_destructor() {
    ASTNode* node = new_node(NODE_DESTRUCTOR);
    if (!node) return NULL;
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after destructor");
        free(node);
        return NULL;
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after destructor parameters");
    }
    
    // Destructor body
    if (!match(TK_LBRACE)) {
        error("Expected '{' before destructor body");
        free(node);
        return NULL;
    }
    
    node->data.func.body = block();
    
    return node;
}

// ======================================================
// [SECTION] STATEMENT PARSING
// ======================================================
static ASTNode* statement();
static ASTNode* declaration();
static ASTNode* block();
static ASTNode* if_statement();
static ASTNode* while_statement();
static ASTNode* for_statement();
static ASTNode* foreach_statement();
static ASTNode* switch_statement();
static ASTNode* try_statement();
static ASTNode* throw_statement();
static ASTNode* return_statement();
static ASTNode* yield_statement();
static ASTNode* print_statement();
static ASTNode* variable_declaration();
static ASTNode* expression_statement();

static ASTNode* statement() {
    if (match(TK_PRINT)) return print_statement();
    if (match(TK_IF)) return if_statement();
    if (match(TK_WHILE)) return while_statement();
    if (match(TK_FOR)) {
        if (check_next(TK_IDENT) && peek_token().kind == TK_IN) {
            return foreach_statement();
        }
        return for_statement();
    }
    if (match(TK_SWITCH)) return switch_statement();
    if (match(TK_TRY)) return try_statement();
    if (match(TK_THROW)) return throw_statement();
    if (match(TK_RETURN)) return return_statement();
    if (match(TK_YIELD)) return yield_statement();
    if (match(TK_BREAK) || match(TK_CONTINUE)) {
        ASTNode* node = new_node(NODE_EMPTY);
        node->op_type = previous.kind;
        if (!match(TK_SEMICOLON)) {
            error("Expected ';' after break/continue");
        }
        return node;
    }
    if (match(TK_LBRACE)) return block();
    
    return expression_statement();
}

static ASTNode* expression_statement() {
    ASTNode* expr = expression();
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after expression");
    }
    return expr;
}

static ASTNode* block() {
    ASTNode* node = new_node(NODE_BLOCK);
    
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

static ASTNode* if_statement() {
    ASTNode* node = new_node(NODE_IF);
    
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

static ASTNode* while_statement() {
    ASTNode* node = new_node(NODE_WHILE);
    
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

static ASTNode* for_statement() {
    ASTNode* node = new_node(NODE_FOR);
    
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
        node->data.loop.init = variable_declaration();
    } else {
        node->data.loop.init = expression_statement();
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
    
    node->data.loop.body = statement(); // Body
    
    return node;
}

static ASTNode* foreach_statement() {
    ASTNode* node = new_node(NODE_FOREACH);
    
    if (!match(TK_IDENT)) {
        error("Expected iteration variable in foreach");
        free(node);
        return NULL;
    }
    node->data.name = str_copy(previous.value.str_val);
    
    if (!match(TK_IN)) {
        error("Expected 'in' in foreach loop");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    node->left = expression(); // Collection
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after foreach condition");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    node->right = statement(); // Body
    
    return node;
}

static ASTNode* switch_statement() {
    ASTNode* node = new_node(NODE_SWITCH);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'switch'");
        free(node);
        return NULL;
    }
    
    node->left = expression(); // Switch expression
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after switch expression");
        free(node);
        return NULL;
    }
    
    if (!match(TK_LBRACE)) {
        error("Expected '{' before switch body");
        free(node);
        return NULL;
    }
    
    // Parse cases
    ASTNode* first_case = NULL;
    ASTNode* current_case = NULL;
    
    while (!check(TK_RBRACE) && !check(TK_EOF)) {
        if (match(TK_CASE) || match(TK_DEFAULT)) {
            ASTNode* case_node = new_node(NODE_CASE);
            if (!case_node) break;
            
            case_node->op_type = previous.kind;
            
            if (previous.kind == TK_CASE) {
                case_node->left = expression(); // Case value
            }
            
            if (!match(TK_COLON)) {
                error("Expected ':' after case");
                free(case_node);
                break;
            }
            
            // Parse case body
            ASTNode* case_body = new_node(NODE_BLOCK);
            while (!check(TK_CASE) && !check(TK_DEFAULT) && 
                   !check(TK_RBRACE) && !check(TK_EOF)) {
                ASTNode* stmt = declaration();
                if (stmt) {
                    if (!case_body->left) {
                        case_body->left = stmt;
                    } else {
                        ASTNode* current = case_body->left;
                        while (current->right) {
                            current = current->right;
                        }
                        current->right = stmt;
                    }
                }
            }
            
            case_node->right = case_body;
            
            if (!first_case) {
                first_case = case_node;
                current_case = case_node;
            } else {
                current_case->third = case_node;
                current_case = case_node;
            }
        } else {
            error("Expected 'case' or 'default' in switch");
            break;
        }
    }
    
    node->right = first_case; // Cases chain
    
    if (!match(TK_RBRACE)) {
        error("Expected '}' after switch body");
    }
    
    return node;
}

static ASTNode* try_statement() {
    ASTNode* node = new_node(NODE_TRY);
    
    node->left = block(); // Try block
    
    if (!match(TK_CATCH)) {
        error("Expected 'catch' after try block");
        free(node);
        return NULL;
    }
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after 'catch'");
        free(node);
        return NULL;
    }
    
    if (!match(TK_IDENT)) {
        error("Expected exception variable name");
        free(node);
        return NULL;
    }
    
    node->data.name = str_copy(previous.value.str_val);
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after catch parameter");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    node->right = block(); // Catch block
    
    if (match(TK_FINALLY)) {
        node->third = block(); // Finally block
    }
    
    return node;
}

static ASTNode* throw_statement() {
    ASTNode* node = new_node(NODE_THROW);
    
    node->left = expression();
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after throw");
    }
    
    return node;
}

static ASTNode* return_statement() {
    ASTNode* node = new_node(NODE_RETURN);
    
    if (!check(TK_SEMICOLON)) {
        node->left = expression();
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after return value");
    }
    
    return node;
}

static ASTNode* yield_statement() {
    ASTNode* node = new_node(NODE_YIELD);
    
    if (!check(TK_SEMICOLON)) {
        node->left = expression();
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after yield value");
    }
    
    return node;
}

static ASTNode* print_statement() {
    ASTNode* node = new_node(NODE_PRINT);
    
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
static ASTNode* variable_declaration() {
    TokenKind decl_type = previous.kind;
    
    if (!match(TK_IDENT)) {
        error("Expected variable name");
        return NULL;
    }
    
    char* var_name = str_copy(previous.value.str_val);
    ASTNode* node = NULL;
    
    switch (decl_type) {
        case TK_VAR: node = new_node(NODE_VAR_DECL); break;
        case TK_NET: node = new_node(NODE_NET_DECL); break;
        case TK_CLOG: node = new_node(NODE_CLOG_DECL); break;
        case TK_DOS: node = new_node(NODE_DOS_DECL); break;
        case TK_SEL: node = new_node(NODE_SEL_DECL); break;
        case TK_LET: 
        case TK_CONST: node = new_node(NODE_CONST_DECL); break;
        default: node = new_node(NODE_VAR_DECL); break;
    }
    
    if (!node) {
        free(var_name);
        return NULL;
    }
    
    node->data.name = var_name;
    node->is_constant = (decl_type == TK_CONST || decl_type == TK_LET);
    
    // Type annotation
    if (match(TK_COLON)) {
        // Skip type for now
        if (!match(TK_IDENT)) {
            error("Expected type name");
        }
    }
    
    // Optional initialization
    if (match(TK_ASSIGN)) {
        node->left = expression();
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after variable declaration");
    }
    
    return node;
}

// ======================================================
// [SECTION] NAMESPACE PARSING
// ======================================================
static ASTNode* parse_namespace() {
    ASTNode* node = new_node(NODE_NAMESPACE);
    
    if (!match(TK_IDENT)) {
        error("Expected namespace name");
        free(node);
        return NULL;
    }
    
    node->data.name = str_copy(previous.value.str_val);
    
    if (!match(TK_LBRACE)) {
        error("Expected '{' before namespace body");
        free(node->data.name);
        free(node);
        return NULL;
    }
    
    node->left = block(); // Namespace body
    
    return node;
}

// ======================================================
// [SECTION] USING DIRECTIVE
// ======================================================
static ASTNode* parse_using() {
    ASTNode* node = new_node(NODE_USING);
    
    if (!match(TK_IDENT)) {
        error("Expected namespace name after using");
        free(node);
        return NULL;
    }
    
    node->data.name = str_copy(previous.value.str_val);
    
    // Optional alias
    if (match(TK_AS)) {
        if (!match(TK_IDENT)) {
            error("Expected alias name after 'as'");
            free(node->data.name);
            free(node);
            return NULL;
        }
        node->data.type_name = str_copy(previous.value.str_val);
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after using directive");
    }
    
    return node;
}

// ======================================================
// [SECTION] MAIN DECLARATION
// ======================================================
static ASTNode* main_declaration() {
    if (!match(TK_LPAREN)) {
        error("Expected '(' after main");
        return NULL;
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after '('");
        return NULL;
    }
    
    ASTNode* node = new_node(NODE_MAIN);
    
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
static ASTNode* dbvar_command() {
    ASTNode* node = new_node(NODE_DBVAR);
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after dbvar");
    }
    
    return node;
}

// ======================================================
// [SECTION] ASSERT STATEMENT
// ======================================================
static ASTNode* parse_assert() {
    ASTNode* node = new_node(NODE_ASSERT);
    
    if (!match(TK_LPAREN)) {
        error("Expected '(' after assert");
        free(node);
        return NULL;
    }
    
    node->left = expression();
    
    if (match(TK_COMMA)) {
        node->right = expression(); // Optional message
    }
    
    if (!match(TK_RPAREN)) {
        error("Expected ')' after assert");
        free(node);
        return NULL;
    }
    
    if (!match(TK_SEMICOLON)) {
        error("Expected ';' after assert");
    }
    
    return node;
}

// ======================================================
// [SECTION] DECLARATION PARSING
// ======================================================
static ASTNode* declaration() {
    if (match(TK_MAIN)) return main_declaration();
    if (match(TK_IMPORT)) return parse_import();
    if (match(TK_EXPORT)) return parse_export();
    if (match(TK_NAMESPACE)) return parse_namespace();
    if (match(TK_USING)) return parse_using();
    if (match(TK_ASSERT)) return parse_assert();
    if (match(TK_DBVAR)) return dbvar_command();
    
    if (match(TK_CLASS) || match(TK_STRUCT) || match(TK_ENUM)) {
        return parse_class();
    }
    
    if (match(TK_FUNC) || match(TK_ASYNC) || match(TK_GENERATOR)) {
        return parse_function();
    }
    
    if (match(TK_VAR) || match(TK_LET) || match(TK_CONST) ||
        match(TK_NET) || match(TK_CLOG) || match(TK_DOS) || match(TK_SEL)) {
        return variable_declaration();
    }
    
    return statement();
}

// ======================================================
// [SECTION] MAIN PARSER FUNCTION
// ======================================================
ASTNode** parse(const char* source, int* count, Error* error) {
    init_lexer(source);
    advance();
    had_error = false;
    panic_mode = false;
    parse_error = error;
    
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
        
        if (panic_mode) {
            synchronize();
        }
        
        if (error && error->message[0]) {
            // Stop on error if requested
            break;
        }
    }
    
    parse_error = NULL;
    return nodes;
}

/*
[file name]: parser.c
[file content begin]
*/


#include "parser.h"
#include "common.h"
#include <stdio.h>

// Forward declarations
static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_assignment(Parser* parser);
static ASTNode* parse_equality(Parser* parser);
static ASTNode* parse_comparison(Parser* parser);
static ASTNode* parse_term(Parser* parser);
static ASTNode* parse_factor(Parser* parser);
static ASTNode* parse_unary(Parser* parser);
static ASTNode* parse_primary(Parser* parser);
static ASTNode* parse_list(Parser* parser);
static ASTNode* parse_map(Parser* parser);
static ASTNode* parse_function_call(Parser* parser);
static ASTNode* parse_expression_statement(Parser* parser);
static ASTNode* parse_return_statement(Parser* parser);
static ASTNode* parse_break_statement(Parser* parser);
static ASTNode* parse_continue_statement(Parser* parser);
static ASTNode* parse_try_statement(Parser* parser);
static ASTNode* parse_switch_statement(Parser* parser);
static ASTNode* parse_class_declaration(Parser* parser);

// Global error tracking for better error messages
static char parser_error_buffer[512] = {0};

void parser_init(Parser* parser, Lexer* lexer) {
    parser->lexer = lexer;
    parser->had_error = false;
    parser->panic_mode = false;
    parser_error_buffer[0] = '\0';
    
    // Read first token
    parser->current = lexer_next_token(lexer);
    parser->previous = parser->current;
}

void parser_error(Parser* parser, Token token, const char* message) {
    if (parser->panic_mode) return;
    
    parser->panic_mode = true;
    
    snprintf(parser_error_buffer, sizeof(parser_error_buffer),
             "Line %d, Column %d: %s", token.line, token.column, message);
    
    fprintf(stderr, "%s[PARSER ERROR]%s %s\n",
            COLOR_RED, COLOR_RESET, parser_error_buffer);
    
    if (token.kind == TK_ERROR) {
        fprintf(stderr, "  Token error: %.*s\n", token.length, token.start);
    } else if (token.kind != TK_EOF) {
        fprintf(stderr, "  At token: %.*s (%s)\n", 
                token.length, token.start, token_kind_to_string(token.kind));
    }
    
    parser->had_error = true;
}

void parser_error_at_current(Parser* parser, const char* message) {
    parser_error(parser, parser->current, message);
}

void parser_error_at_previous(Parser* parser, const char* message) {
    parser_error(parser, parser->previous, message);
}

bool parser_match(Parser* parser, TokenKind kind) {
    if (!parser_check(parser, kind)) return false;
    parser_advance(parser);
    return true;
}

bool parser_check(Parser* parser, TokenKind kind) {
    return parser->current.kind == kind;
}

Token parser_advance(Parser* parser) {
    parser->previous = parser->current;
    
    if (!parser->panic_mode) {
        parser->current = lexer_next_token(parser->lexer);
        
        if (parser->current.kind == TK_ERROR) {
            parser_error_at_current(parser, "Lexical error");
        }
    }
    
    return parser->previous;
}

Token parser_consume(Parser* parser, TokenKind kind, const char* error_message) {
    if (parser_check(parser, kind)) {
        return parser_advance(parser);
    }
    
    // Build better error message
    char detailed_error[256];
    snprintf(detailed_error, sizeof(detailed_error),
             "%s. Found: %s", error_message, 
             token_kind_to_string(parser->current.kind));
    
    parser_error_at_current(parser, detailed_error);
    
    Token error_token;
    error_token.kind = TK_ERROR;
    error_token.start = error_message;
    error_token.length = strlen(error_message);
    error_token.line = parser->current.line;
    error_token.column = parser->current.column;
    return error_token;
}

void parser_synchronize(Parser* parser) {
    parser->panic_mode = false;
    
    // Skip tokens until we find a statement boundary
    while (parser->current.kind != TK_EOF) {
        if (parser->previous.kind == TK_SEMICOLON) return;
        
        switch (parser->current.kind) {
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
            case TK_CLASS:
            case TK_IMPORT:
            case TK_EXPORT:
            case TK_TRY:
            case TK_THROW:
                return;
            default:
                break;
        }
        
        parser_advance(parser);
    }
}

ASTNode* parse_program(Parser* parser) {
    ASTNode* program = ast_new_node(NODE_PROGRAM, 1, 1);
    ASTNode* statements = NULL;
    ASTNode** current = &statements;
    
    while (!parser_check(parser, TK_EOF) && !parser->had_error) {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            *current = stmt;
            current = &((*current)->right);
        }
        
        // Optional semicolon
        parser_match(parser, TK_SEMICOLON);
        
        if (parser->panic_mode) {
            parser_synchronize(parser);
        }
    }
    
    if (!parser->had_error) {
        parser_consume(parser, TK_EOF, "Expected end of file");
    }
    
    program->left = statements;
    return program;
}

ASTNode* parse_statement(Parser* parser) {
    if (parser_match(parser, TK_PRINT)) {
        return parse_print_statement(parser);
    } else if (parser_match(parser, TK_IF)) {
        return parse_if_statement(parser);
    } else if (parser_match(parser, TK_WHILE)) {
        return parse_while_statement(parser);
    } else if (parser_match(parser, TK_FOR)) {
        return parse_for_statement(parser);
    } else if (parser_match(parser, TK_RETURN)) {
        return parse_return_statement(parser);
    } else if (parser_match(parser, TK_BREAK)) {
        return parse_break_statement(parser);
    } else if (parser_match(parser, TK_CONTINUE)) {
        return parse_continue_statement(parser);
    } else if (parser_match(parser, TK_IMPORT)) {
        return parse_import_statement(parser);
    } else if (parser_match(parser, TK_TRY)) {
        return parse_try_statement(parser);
    } else if (parser_match(parser, TK_THROW)) {
        return parse_throw_statement(parser);
    } else if (parser_match(parser, TK_SWITCH)) {
        return parse_switch_statement(parser);
    } else if (parser_match(parser, TK_CLASS)) {
        return parse_class_declaration(parser);
    } else if (parser_match(parser, TK_VAR) || parser_match(parser, TK_NET) || 
               parser_match(parser, TK_CLOG) || parser_match(parser, TK_DOS) || 
               parser_match(parser, TK_SEL) || parser_match(parser, TK_CONST) ||
               parser_match(parser, TK_LET) || parser_match(parser, TK_GLOBAL)) {
        return parse_var_declaration(parser);
    } else if (parser_match(parser, TK_FUNC)) {
        return parse_function_declaration(parser);
    } else if (parser_match(parser, TK_LBRACE)) {
        return parse_block(parser);
    } else if (parser_match(parser, TK_PASS)) {
        return ast_new_node(NODE_PASS, parser->previous.line, parser->previous.column);
    } else {
        return parse_expression_statement(parser);
    }
}

ASTNode* parse_print_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* value = parse_expression(parser);
    if (!value) {
        parser_error_at_current(parser, "Expected expression after 'print'");
        return NULL;
    }
    
    return ast_new_print(value, line, column);
}

ASTNode* parse_if_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    // SwiftFlow syntax: if [condition]
    parser_consume(parser, TK_LSQUARE, "Expected '[' after 'if'");
    ASTNode* condition = parse_expression(parser);
    if (!condition) {
        parser_error_at_current(parser, "Expected condition");
        return NULL;
    }
    parser_consume(parser, TK_RSQUARE, "Expected ']' after condition");
    
    // Parse then branch
    ASTNode* then_branch = parse_statement(parser);
    if (!then_branch) {
        parser_error_at_current(parser, "Expected statement after if condition");
        return NULL;
    }
    
    // Parse optional else branch
    ASTNode* else_branch = NULL;
    if (parser_match(parser, TK_ELSE)) {
        else_branch = parse_statement(parser);
        if (!else_branch) {
            parser_error_at_current(parser, "Expected statement after 'else'");
        }
    }
    
    return ast_new_if(condition, then_branch, else_branch, line, column);
}

ASTNode* parse_while_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    // SwiftFlow syntax: while [condition]
    parser_consume(parser, TK_LSQUARE, "Expected '[' after 'while'");
    ASTNode* condition = parse_expression(parser);
    if (!condition) {
        parser_error_at_current(parser, "Expected condition");
        return NULL;
    }
    parser_consume(parser, TK_RSQUARE, "Expected ']' after condition");
    
    ASTNode* body = parse_statement(parser);
    if (!body) {
        parser_error_at_current(parser, "Expected statement after while condition");
        return NULL;
    }
    
    return ast_new_while(condition, body, line, column);
}

ASTNode* parse_for_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    // SwiftFlow syntax: for [init; condition; update]
    parser_consume(parser, TK_LSQUARE, "Expected '[' after 'for'");
    
    ASTNode* init = NULL;
    if (!parser_check(parser, TK_SEMICOLON)) {
        init = parse_expression(parser);
    }
    parser_consume(parser, TK_SEMICOLON, "Expected ';' after for loop initializer");
    
    ASTNode* condition = NULL;
    if (!parser_check(parser, TK_SEMICOLON)) {
        condition = parse_expression(parser);
    }
    parser_consume(parser, TK_SEMICOLON, "Expected ';' after for loop condition");
    
    ASTNode* update = NULL;
    if (!parser_check(parser, TK_RSQUARE)) {
        update = parse_expression(parser);
    }
    parser_consume(parser, TK_RSQUARE, "Expected ']' after for loop update");
    
    ASTNode* body = parse_statement(parser);
    if (!body) {
        parser_error_at_current(parser, "Expected statement after for loop");
        return NULL;
    }
    
    return ast_new_for(init, condition, update, body, line, column);
}

static ASTNode* parse_return_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* value = NULL;
    if (!parser_check(parser, TK_SEMICOLON)) {
        value = parse_expression(parser);
    }
    
    return ast_new_return(value, line, column);
}

static ASTNode* parse_break_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    return ast_new_node(NODE_BREAK, line, column);
}

static ASTNode* parse_continue_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    return ast_new_node(NODE_CONTINUE, line, column);
}

ASTNode* parse_throw_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* value = parse_expression(parser);
    if (!value) {
        parser_error_at_current(parser, "Expected expression after 'throw'");
        return NULL;
    }
    
    ASTNode* node = ast_new_node(NODE_THROW, line, column);
    node->left = value;
    return node;
}

ASTNode* parse_try_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* try_block = parse_block(parser);
    if (!try_block) {
        parser_error_at_current(parser, "Expected try block");
        return NULL;
    }
    
    ASTNode* catch_block = NULL;
    if (parser_match(parser, TK_CATCH)) {
        parser_consume(parser, TK_LPAREN, "Expected '(' after 'catch'");
        Token error_var = parser_consume(parser, TK_IDENT, "Expected error variable name");
        parser_consume(parser, TK_RPAREN, "Expected ')' after catch parameter");
        
        catch_block = parse_block(parser);
        if (!catch_block) {
            parser_error_at_current(parser, "Expected catch block");
        }
    }
    
    ASTNode* finally_block = NULL;
    if (parser_match(parser, TK_FINALLY)) {
        finally_block = parse_block(parser);
        if (!finally_block) {
            parser_error_at_current(parser, "Expected finally block");
        }
    }
    
    ASTNode* node = ast_new_node(NODE_TRY, line, column);
    node->left = try_block;
    node->right = catch_block;
    node->third = finally_block;
    return node;
}

ASTNode* parse_switch_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_LPAREN, "Expected '(' after 'switch'");
    ASTNode* value = parse_expression(parser);
    if (!value) {
        parser_error_at_current(parser, "Expected switch expression");
        return NULL;
    }
    parser_consume(parser, TK_RPAREN, "Expected ')' after switch expression");
    
    parser_consume(parser, TK_LBRACE, "Expected '{' to start switch block");
    
    ASTNode* switch_node = ast_new_node(NODE_SWITCH, line, column);
    ASTNode* cases = NULL;
    ASTNode** current_case = &cases;
    
    while (!parser_check(parser, TK_RBRACE) && !parser_check(parser, TK_EOF)) {
        if (parser_match(parser, TK_CASE)) {
            ASTNode* case_value = parse_expression(parser);
            if (!case_value) {
                parser_error_at_current(parser, "Expected case value");
                break;
            }
            
            parser_consume(parser, TK_COLON, "Expected ':' after case value");
            
            ASTNode* case_stmt = parse_statement(parser);
            if (!case_stmt) {
                parser_error_at_current(parser, "Expected statement after case");
                break;
            }
            
            ASTNode* case_node = ast_new_node(NODE_CASE, case_value->line, case_value->column);
            case_node->left = case_value;
            case_node->right = case_stmt;
            
            *current_case = case_node;
            current_case = &((*current_case)->right);
        } else if (parser_match(parser, TK_DEFAULT)) {
            parser_consume(parser, TK_COLON, "Expected ':' after default");
            
            ASTNode* default_stmt = parse_statement(parser);
            if (!default_stmt) {
                parser_error_at_current(parser, "Expected statement after default");
                break;
            }
            
            ASTNode* default_node = ast_new_node(NODE_CASE, parser->previous.line, parser->previous.column);
            default_node->right = default_stmt;
            
            *current_case = default_node;
            current_case = &((*current_case)->right);
        } else {
            parser_error_at_current(parser, "Expected 'case' or 'default'");
            break;
        }
    }
    
    parser_consume(parser, TK_RBRACE, "Expected '}' to end switch block");
    
    switch_node->left = value;
    switch_node->right = cases;
    return switch_node;
}

ASTNode* parse_import_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    Token module_token = parser_consume(parser, TK_STRING, "Expected module name string");
    if (module_token.kind == TK_ERROR) return NULL;
    
    char* module_name = str_ncopy(module_token.start + 1, module_token.length - 2);
    
    if (parser_match(parser, TK_FROM)) {
        Token from_token = parser_consume(parser, TK_STRING, "Expected source string after 'from'");
        if (from_token.kind == TK_ERROR) {
            free(module_name);
            return NULL;
        }
        
        char* from_name = str_ncopy(from_token.start + 1, from_token.length - 2);
        char** modules = malloc(sizeof(char*));
        modules[0] = module_name;
        
        ASTNode* import_node = ast_new_import(modules, 1, from_name, line, column);
        free(from_name);
        return import_node;
    } else {
        char** modules = malloc(sizeof(char*));
        modules[0] = module_name;
        return ast_new_import(modules, 1, NULL, line, column);
    }
}

ASTNode* parse_var_declaration(Parser* parser) {
    TokenKind var_type = parser->previous.kind;
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    Token name_token = parser_consume(parser, TK_IDENT, "Expected variable name");
    if (name_token.kind == TK_ERROR) return NULL;
    
    char* var_name = str_ncopy(name_token.start, name_token.length);
    
    ASTNode* initializer = NULL;
    if (parser_match(parser, TK_ASSIGN)) {
        initializer = parse_expression(parser);
        if (!initializer) {
            parser_error_at_current(parser, "Expected expression after '='");
            free(var_name);
            return NULL;
        }
    }
    
    return ast_new_var_decl(var_name, initializer, var_type, line, column);
}

ASTNode* parse_function_declaration(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    Token name_token = parser_consume(parser, TK_IDENT, "Expected function name");
    if (name_token.kind == TK_ERROR) return NULL;
    
    char* func_name = str_ncopy(name_token.start, name_token.length);
    
    parser_consume(parser, TK_LPAREN, "Expected '(' after function name");
    
    ASTNode* params = NULL;
    ASTNode** current_param = &params;
    
    if (!parser_check(parser, TK_RPAREN)) {
        do {
            Token param_token = parser_consume(parser, TK_IDENT, "Expected parameter name");
            if (param_token.kind == TK_ERROR) {
                free(func_name);
                return NULL;
            }
            
            char* param_name = str_ncopy(param_token.start, param_token.length);
            ASTNode* param_node = ast_new_identifier(param_name, param_token.line, param_token.column);
            free(param_name);
            
            *current_param = param_node;
            current_param = &((*current_param)->right);
            
        } while (parser_match(parser, TK_COMMA));
    }
    
    parser_consume(parser, TK_RPAREN, "Expected ')' after parameters");
    
    ASTNode* body = parse_block(parser);
    if (!body) {
        parser_error_at_current(parser, "Expected function body");
        free(func_name);
        return NULL;
    }
    
    return ast_new_function(func_name, params, body, line, column);
}

static ASTNode* parse_class_declaration(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    Token name_token = parser_consume(parser, TK_IDENT, "Expected class name");
    if (name_token.kind == TK_ERROR) return NULL;
    
    char* class_name = str_ncopy(name_token.start, name_token.length);
    
    ASTNode* parent = NULL;
    if (parser_match(parser, TK_COLON)) {
        Token parent_token = parser_consume(parser, TK_IDENT, "Expected parent class name");
        if (parent_token.kind != TK_ERROR) {
            parent = ast_new_identifier(parent_token.start, parent_token.line, parent_token.column);
        }
    }
    
    parser_consume(parser, TK_LBRACE, "Expected '{' to start class body");
    
    ASTNode* class_node = ast_new_node(NODE_CLASS, line, column);
    class_node->data.name = class_name;
    class_node->left = parent;
    
    ASTNode* members = NULL;
    ASTNode** current_member = &members;
    
    while (!parser_check(parser, TK_RBRACE) && !parser_check(parser, TK_EOF)) {
        if (parser_match(parser, TK_FUNC)) {
            ASTNode* method = parse_function_declaration(parser);
            if (method) {
                *current_member = method;
                current_member = &((*current_member)->right);
            }
        } else if (parser_match(parser, TK_VAR) || parser_match(parser, TK_CONST)) {
            ASTNode* field = parse_var_declaration(parser);
            if (field) {
                *current_member = field;
                current_member = &((*current_member)->right);
            }
        } else {
            parser_error_at_current(parser, "Expected method or field declaration");
            parser_advance(parser);
        }
    }
    
    parser_consume(parser, TK_RBRACE, "Expected '}' to end class body");
    
    class_node->right = members;
    return class_node;
}

ASTNode* parse_block(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_LBRACE, "Expected '{' to start block");
    
    ASTNode* block = ast_new_node(NODE_BLOCK, line, column);
    ASTNode* statements = NULL;
    ASTNode** current = &statements;
    
    while (!parser_check(parser, TK_RBRACE) && !parser_check(parser, TK_EOF)) {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            *current = stmt;
            current = &((*current)->right);
        }
        
        parser_match(parser, TK_SEMICOLON);
        
        if (parser->panic_mode) {
            parser_synchronize(parser);
        }
    }
    
    parser_consume(parser, TK_RBRACE, "Expected '}' to end block");
    block->left = statements;
    return block;
}

static ASTNode* parse_expression_statement(Parser* parser) {
    ASTNode* expr = parse_expression(parser);
    if (!expr) {
        parser_error_at_current(parser, "Expected expression");
        return NULL;
    }
    return expr;
}

static ASTNode* parse_expression(Parser* parser) {
    return parse_assignment(parser);
}

static ASTNode* parse_assignment(Parser* parser) {
    ASTNode* expr = parse_equality(parser);
    
    if (parser_match(parser, TK_ASSIGN) || 
        parser_match(parser, TK_PLUS_ASSIGN) ||
        parser_match(parser, TK_MINUS_ASSIGN) ||
        parser_match(parser, TK_MULT_ASSIGN) ||
        parser_match(parser, TK_DIV_ASSIGN) ||
        parser_match(parser, TK_MOD_ASSIGN) ||
        parser_match(parser, TK_POW_ASSIGN)) {
        
        Token op = parser->previous;
        ASTNode* value = parse_assignment(parser);
        
        if (expr->type == NODE_IDENT) {
            ASTNode* assign = ast_new_node(NODE_ASSIGN, op.line, op.column);
            assign->op_type = op.kind;
            assign->left = expr;
            assign->right = value;
            return assign;
        } else {
            parser_error(parser, op, "Invalid assignment target");
            ast_free(expr);
            ast_free(value);
            return NULL;
        }
    }
    
    return expr;
}

static ASTNode* parse_equality(Parser* parser) {
    ASTNode* expr = parse_comparison(parser);
    
    while (parser_match(parser, TK_EQ) || parser_match(parser, TK_NEQ) ||
           parser_match(parser, TK_IS) || parser_match(parser, TK_ISNOT)) {
        Token op = parser->previous;
        ASTNode* right = parse_comparison(parser);
        
        ASTNode* binary = ast_new_node(NODE_BINARY, op.line, op.column);
        binary->op_type = op.kind;
        binary->left = expr;
        binary->right = right;
        expr = binary;
    }
    
    return expr;
}

static ASTNode* parse_comparison(Parser* parser) {
    ASTNode* expr = parse_term(parser);
    
    while (parser_match(parser, TK_LT) || parser_match(parser, TK_GT) ||
           parser_match(parser, TK_LTE) || parser_match(parser, TK_GTE)) {
        Token op = parser->previous;
        ASTNode* right = parse_term(parser);
        
        ASTNode* binary = ast_new_node(NODE_BINARY, op.line, op.column);
        binary->op_type = op.kind;
        binary->left = expr;
        binary->right = right;
        expr = binary;
    }
    
    return expr;
}

static ASTNode* parse_term(Parser* parser) {
    ASTNode* expr = parse_factor(parser);
    
    while (parser_match(parser, TK_PLUS) || parser_match(parser, TK_MINUS) ||
           parser_match(parser, TK_CONCAT)) {
        Token op = parser->previous;
        ASTNode* right = parse_factor(parser);
        
        ASTNode* binary = ast_new_node(NODE_BINARY, op.line, op.column);
        binary->op_type = op.kind;
        binary->left = expr;
        binary->right = right;
        expr = binary;
    }
    
    return expr;
}

static ASTNode* parse_factor(Parser* parser) {
    ASTNode* expr = parse_unary(parser);
    
    while (parser_match(parser, TK_MULT) || parser_match(parser, TK_DIV) || 
           parser_match(parser, TK_MOD) || parser_match(parser, TK_POW)) {
        Token op = parser->previous;
        ASTNode* right = parse_unary(parser);
        
        ASTNode* binary = ast_new_node(NODE_BINARY, op.line, op.column);
        binary->op_type = op.kind;
        binary->left = expr;
        binary->right = right;
        expr = binary;
    }
    
    return expr;
}

static ASTNode* parse_unary(Parser* parser) {
    if (parser_match(parser, TK_NOT) || parser_match(parser, TK_MINUS) ||
        parser_match(parser, TK_BIT_NOT) || parser_match(parser, TK_INCREMENT) ||
        parser_match(parser, TK_DECREMENT)) {
        Token op = parser->previous;
        ASTNode* right = parse_unary(parser);
        
        ASTNode* unary = ast_new_node(NODE_UNARY, op.line, op.column);
        unary->op_type = op.kind;
        unary->left = right;
        return unary;
    }
    
    return parse_primary(parser);
}

static ASTNode* parse_primary(Parser* parser) {
    if (parser_match(parser, TK_FALSE)) {
        return ast_new_bool(false, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_TRUE)) {
        return ast_new_bool(true, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_NULL)) {
        return ast_new_node(NODE_NULL, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_UNDEFINED)) {
        return ast_new_node(NODE_UNDEFINED, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_NAN)) {
        return ast_new_node(NODE_NAN, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_INF)) {
        return ast_new_node(NODE_INF, parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_INT)) {
        return ast_new_int(parser->previous.value.int_val, 
                          parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_FLOAT)) {
        return ast_new_float(parser->previous.value.float_val,
                            parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_STRING)) {
        return ast_new_string(parser->previous.value.str_val,
                             parser->previous.line, parser->previous.column);
    }
    
    if (parser_match(parser, TK_IDENT)) {
        Token ident = parser->previous;
        char* name = str_ncopy(ident.start, ident.length);
        ASTNode* node = ast_new_identifier(name, ident.line, ident.column);
        free(name);
        
        // Check for function call
        if (parser_check(parser, TK_LPAREN)) {
            return parse_function_call(parser);
        }
        
        // Check for member access
        if (parser_match(parser, TK_PERIOD)) {
            Token member = parser_consume(parser, TK_IDENT, "Expected member name");
            if (member.kind == TK_ERROR) {
                ast_free(node);
                return NULL;
            }
            
            ASTNode* member_node = ast_new_node(NODE_MEMBER_ACCESS, ident.line, ident.column);
            member_node->left = node;
            member_node->right = ast_new_identifier(member.start, member.line, member.column);
            return member_node;
        }
        
        return node;
    }
    
    if (parser_match(parser, TK_LPAREN)) {
        ASTNode* expr = parse_expression(parser);
        parser_consume(parser, TK_RPAREN, "Expected ')' after expression");
        return expr;
    }
    
    if (parser_match(parser, TK_LSQUARE)) {
        return parse_list(parser);
    }
    
    if (parser_match(parser, TK_LBRACE)) {
        return parse_map(parser);
    }
    
    if (parser_match(parser, TK_INPUT)) {
        return parse_input_statement(parser);
    }
    
    if (parser_match(parser, TK_LAMBDA)) {
        int line = parser->previous.line;
        int column = parser->previous.column;
        
        parser_consume(parser, TK_LPAREN, "Expected '(' after lambda");
        
        ASTNode* params = NULL;
        ASTNode** current_param = &params;
        
        if (!parser_check(parser, TK_RPAREN)) {
            do {
                Token param_token = parser_consume(parser, TK_IDENT, "Expected parameter name");
                if (param_token.kind == TK_ERROR) {
                    return NULL;
                }
                
                char* param_name = str_ncopy(param_token.start, param_token.length);
                ASTNode* param_node = ast_new_identifier(param_name, param_token.line, param_token.column);
                free(param_name);
                
                *current_param = param_node;
                current_param = &((*current_param)->right);
                
            } while (parser_match(parser, TK_COMMA));
        }
        
        parser_consume(parser, TK_RPAREN, "Expected ')' after lambda parameters");
        parser_consume(parser, TK_RARROW, "Expected '=>' after lambda parameters");
        
        ASTNode* body = parse_expression(parser);
        if (!body) {
            parser_error_at_current(parser, "Expected lambda body");
            return NULL;
        }
        
        ASTNode* lambda = ast_new_node(NODE_LAMBDA, line, column);
        lambda->left = params;
        lambda->right = body;
        return lambda;
    }
    
    parser_error_at_current(parser, "Expected expression");
    return NULL;
}

static ASTNode* parse_list(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* list = ast_new_node(NODE_LIST, line, column);
    ASTNode* elements = NULL;
    ASTNode** current = &elements;
    
    if (!parser_check(parser, TK_RSQUARE)) {
        do {
            ASTNode* element = parse_expression(parser);
            if (!element) {
                ast_free(list);
                return NULL;
            }
            
            *current = element;
            current = &((*current)->right);
        } while (parser_match(parser, TK_COMMA));
    }
    
    parser_consume(parser, TK_RSQUARE, "Expected ']' after list");
    list->left = elements;
    return list;
}

static ASTNode* parse_map(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* map = ast_new_node(NODE_MAP, line, column);
    ASTNode* entries = NULL;
    ASTNode** current = &entries;
    
    if (!parser_check(parser, TK_RBRACE)) {
        do {
            // Parse key
            ASTNode* key = parse_expression(parser);
            if (!key) {
                ast_free(map);
                return NULL;
            }
            
            parser_consume(parser, TK_COLON, "Expected ':' after map key");
            
            // Parse value
            ASTNode* value = parse_expression(parser);
            if (!value) {
                ast_free(key);
                ast_free(map);
                return NULL;
            }
            
            // Create key-value pair
            ASTNode* pair = ast_new_node(NODE_ASSIGN, key->line, key->column);
            pair->left = key;
            pair->right = value;
            
            *current = pair;
            current = &((*current)->right);
            
        } while (parser_match(parser, TK_COMMA));
    }
    
    parser_consume(parser, TK_RBRACE, "Expected '}' after map");
    map->left = entries;
    return map;
}

static ASTNode* parse_function_call(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* func_call = ast_new_node(NODE_FUNC_CALL, line, column);
    
    // Parse arguments
    parser_consume(parser, TK_LPAREN, "Expected '(' after function name");
    
    ASTNode* args = NULL;
    ASTNode** current_arg = &args;
    
    if (!parser_check(parser, TK_RPAREN)) {
        do {
            ASTNode* arg = parse_expression(parser);
            if (!arg) {
                ast_free(func_call);
                return NULL;
            }
            
            *current_arg = arg;
            current_arg = &((*current_arg)->right);
        } while (parser_match(parser, TK_COMMA));
    }
    
    parser_consume(parser, TK_RPAREN, "Expected ')' after arguments");
    
    // The function name is in previous token (before '(')
    func_call->left = ast_new_identifier(parser->previous.start, parser->previous.line, parser->previous.column);
    func_call->right = args;
    
    return func_call;
}

ASTNode* parse_input_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    parser_consume(parser, TK_LPAREN, "Expected '(' after 'input'");
    
    char* prompt = NULL;
    if (parser_check(parser, TK_STRING)) {
        Token prompt_token = parser_advance(parser);
        prompt = str_ncopy(prompt_token.start + 1, prompt_token.length - 2);
    }
    
    parser_consume(parser, TK_RPAREN, "Expected ')' after input prompt");
    return ast_new_input(prompt, line, column);
}
[file content end]

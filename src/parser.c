#include "parser.h"
#include "common.h"
#include <stdio.h>

// Forward declarations pour les fonctions internes
static ASTNode* parse_expression(Parser* parser);
static ASTNode* parse_assignment(Parser* parser);
static ASTNode* parse_equality(Parser* parser);
static ASTNode* parse_comparison(Parser* parser);
static ASTNode* parse_term(Parser* parser);
static ASTNode* parse_factor(Parser* parser);
static ASTNode* parse_unary(Parser* parser);
static ASTNode* parse_primary(Parser* parser);
static ASTNode* parse_list(Parser* parser);
static ASTNode* parse_function_call(Parser* parser);
static ASTNode* parse_expression_statement(Parser* parser);
static ASTNode* parse_return_statement(Parser* parser);
static ASTNode* parse_break_statement(Parser* parser);
static ASTNode* parse_continue_statement(Parser* parser);

// Parser initialization
void parser_init(Parser* parser, Lexer* lexer) {
    parser->lexer = lexer;
    parser->had_error = false;
    parser->panic_mode = false;
    
    // Read first token
    parser->current = lexer_next_token(lexer);
    parser->previous = parser->current;
}

// Error handling
void parser_error(Parser* parser, Token token, const char* message) {
    if (parser->panic_mode) return;
    
    parser->panic_mode = true;
    fprintf(stderr, "%s[PARSER ERROR]%s Line %d, Column %d: %s\n",
            COLOR_RED, COLOR_RESET, token.line, token.column, message);
    
    if (token.kind == TK_ERROR) {
        fprintf(stderr, "  Token error: %.*s\n", token.length, token.start);
    }
    
    parser->had_error = true;
}

void parser_error_at_current(Parser* parser, const char* message) {
    parser_error(parser, parser->current, message);
}

void parser_error_at_previous(Parser* parser, const char* message) {
    parser_error(parser, parser->previous, message);
}

// Token consumption
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
            parser_error_at_current(parser, "Lexer error");
        }
    }
    
    return parser->previous;
}

Token parser_consume(Parser* parser, TokenKind kind, const char* error_message) {
    if (parser_check(parser, kind)) {
        return parser_advance(parser);
    }
    
    parser_error_at_current(parser, error_message);
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
                return;
            default:
                break;
        }
        
        parser_advance(parser);
    }
}

// Parse a complete program
ASTNode* parse_program(Parser* parser) {
    ASTNode* program = ast_new_node(NODE_PROGRAM, 1, 1);
    ASTNode* statements = NULL;
    ASTNode** current = &statements;
    
    while (!parser_check(parser, TK_EOF) && !parser->had_error) {
        ASTNode* stmt = parse_statement(parser);
        if (stmt) {
            *current = stmt;
            current = &((*current)->right); // Link statements
        }
        
        // Optional semicolon
        parser_match(parser, TK_SEMICOLON);
        
        if (parser->panic_mode) {
            parser_synchronize(parser);
        }
    }
    
    parser_consume(parser, TK_EOF, "Expected end of file");
    program->left = statements;
    return program;
}

// Parse a statement
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
    } else if (parser_match(parser, TK_VAR) || parser_match(parser, TK_NET) || 
               parser_match(parser, TK_CLOG) || parser_match(parser, TK_DOS) || 
               parser_match(parser, TK_SEL) || parser_match(parser, TK_CONST) ||
               parser_match(parser, TK_LET)) {
        return parse_var_declaration(parser);
    } else if (parser_match(parser, TK_FUNC)) {
        return parse_function_declaration(parser);
    } else if (parser_match(parser, TK_LBRACE)) {
        return parse_block(parser);
    } else {
        return parse_expression_statement(parser);
    }
}

// Parse print statement
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

// Parse if statement
ASTNode* parse_if_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    // Parse condition with SwiftFlow syntax: if [condition]
    parser_consume(parser, TK_LSQUARE, "Expected '[' after 'if'");
    ASTNode* condition = parse_expression(parser);
    parser_consume(parser, TK_RSQUARE, "Expected ']' after condition");
    
    if (!condition) {
        parser_error_at_current(parser, "Expected condition");
        return NULL;
    }
    
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

// Parse while statement
ASTNode* parse_while_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    // Parse condition with SwiftFlow syntax: while [condition]
    parser_consume(parser, TK_LSQUARE, "Expected '[' after 'while'");
    ASTNode* condition = parse_expression(parser);
    parser_consume(parser, TK_RSQUARE, "Expected ']' after condition");
    
    if (!condition) {
        parser_error_at_current(parser, "Expected condition");
        return NULL;
    }
    
    // Parse body
    ASTNode* body = parse_statement(parser);
    if (!body) {
        parser_error_at_current(parser, "Expected statement after while condition");
        return NULL;
    }
    
    return ast_new_while(condition, body, line, column);
}

// Parse for statement
ASTNode* parse_for_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    // Parse for loop with SwiftFlow syntax: for [init; condition; update]
    parser_consume(parser, TK_LSQUARE, "Expected '[' after 'for'");
    
    // Parse initialization (optional)
    ASTNode* init = NULL;
    if (!parser_check(parser, TK_SEMICOLON)) {
        init = parse_expression(parser);
    }
    parser_consume(parser, TK_SEMICOLON, "Expected ';' after for loop initializer");
    
    // Parse condition (optional)
    ASTNode* condition = NULL;
    if (!parser_check(parser, TK_SEMICOLON)) {
        condition = parse_expression(parser);
    }
    parser_consume(parser, TK_SEMICOLON, "Expected ';' after for loop condition");
    
    // Parse update (optional)
    ASTNode* update = NULL;
    if (!parser_check(parser, TK_RSQUARE)) {
        update = parse_expression(parser);
    }
    parser_consume(parser, TK_RSQUARE, "Expected ']' after for loop update");
    
    // Parse body
    ASTNode* body = parse_statement(parser);
    if (!body) {
        parser_error_at_current(parser, "Expected statement after for loop");
        return NULL;
    }
    
    return ast_new_for(init, condition, update, body, line, column);
}

// Parse return statement
static ASTNode* parse_return_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    ASTNode* value = NULL;
    if (!parser_check(parser, TK_SEMICOLON)) {
        value = parse_expression(parser);
    }
    
    return ast_new_return(value, line, column);
}

// Parse break statement
static ASTNode* parse_break_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    return ast_new_node(NODE_BREAK, line, column);
}

// Parse continue statement
static ASTNode* parse_continue_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    return ast_new_node(NODE_CONTINUE, line, column);
}

// Parse import statement
ASTNode* parse_import_statement(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    // Parse module name
    Token module_token = parser_consume(parser, TK_STRING, "Expected module name string");
    if (module_token.kind == TK_ERROR) return NULL;
    
    char* module_name = str_ncopy(module_token.start + 1, module_token.length - 2);
    
    // Check for "from" keyword
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
        // Simple import
        char** modules = malloc(sizeof(char*));
        modules[0] = module_name;
        return ast_new_import(modules, 1, NULL, line, column);
    }
}

// Parse variable declaration
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

// Parse function declaration
ASTNode* parse_function_declaration(Parser* parser) {
    int line = parser->previous.line;
    int column = parser->previous.column;
    
    Token name_token = parser_consume(parser, TK_IDENT, "Expected function name");
    if (name_token.kind == TK_ERROR) return NULL;
    
    char* func_name = str_ncopy(name_token.start, name_token.length);
    
    // Parse parameters
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
    
    // Parse body
    ASTNode* body = parse_block(parser);
    if (!body) {
        parser_error_at_current(parser, "Expected function body");
        free(func_name);
        return NULL;
    }
    
    return ast_new_function(func_name, params, body, line, column);
}

// Parse block statement
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
        
        // Optional semicolon
        parser_match(parser, TK_SEMICOLON);
        
        if (parser->panic_mode) {
            parser_synchronize(parser);
        }
    }
    
    parser_consume(parser, TK_RBRACE, "Expected '}' to end block");
    block->left = statements;
    return block;
}

// Parse expression statement
static ASTNode* parse_expression_statement(Parser* parser) {
    ASTNode* expr = parse_expression(parser);
    if (!expr) {
        parser_error_at_current(parser, "Expected expression");
        return NULL;
    }
    return expr;
}

// Parse expression
static ASTNode* parse_expression(Parser* parser) {
    return parse_assignment(parser);
}

// Parse assignment
static ASTNode* parse_assignment(Parser* parser) {
    ASTNode* expr = parse_equality(parser);
    
    if (parser_match(parser, TK_ASSIGN)) {
        Token equals = parser->previous;
        ASTNode* value = parse_assignment(parser);
        
        if (expr->type == NODE_IDENT) {
            return ast_new_assignment(expr, value, equals.line, equals.column);
        } else {
            parser_error(parser, equals, "Invalid assignment target");
            ast_free(expr);
            ast_free(value);
            return NULL;
        }
    }
    
    return expr;
}

// Parse equality (==, !=)
static ASTNode* parse_equality(Parser* parser) {
    ASTNode* expr = parse_comparison(parser);
    
    while (parser_match(parser, TK_EQ) || parser_match(parser, TK_NEQ)) {
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

// Parse comparison (<, >, <=, >=)
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

// Parse term (+, -)
static ASTNode* parse_term(Parser* parser) {
    ASTNode* expr = parse_factor(parser);
    
    while (parser_match(parser, TK_PLUS) || parser_match(parser, TK_MINUS)) {
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

// Parse factor (*, /, %)
static ASTNode* parse_factor(Parser* parser) {
    ASTNode* expr = parse_unary(parser);
    
    while (parser_match(parser, TK_MULT) || parser_match(parser, TK_DIV) || 
           parser_match(parser, TK_MOD)) {
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

// Parse unary (!, -)
static ASTNode* parse_unary(Parser* parser) {
    if (parser_match(parser, TK_NOT) || parser_match(parser, TK_MINUS)) {
        Token op = parser->previous;
        ASTNode* right = parse_unary(parser);
        
        ASTNode* unary = ast_new_node(NODE_UNARY, op.line, op.column);
        unary->op_type = op.kind;
        unary->left = right;
        return unary;
    }
    
    return parse_primary(parser);
}

// Parse primary expressions
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
    
    if (parser_match(parser, TK_INPUT)) {
        parser_consume(parser, TK_LPAREN, "Expected '(' after 'input'");
        
        char* prompt = NULL;
        if (parser_check(parser, TK_STRING)) {
            Token prompt_token = parser_advance(parser);
            prompt = str_ncopy(prompt_token.start + 1, prompt_token.length - 2);
        }
        
        parser_consume(parser, TK_RPAREN, "Expected ')' after input prompt");
        return ast_new_input(prompt, parser->previous.line, parser->previous.column);
    }
    
    parser_error_at_current(parser, "Expected expression");
    return NULL;
}

// Parse list literal
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

// Parse function call
static ASTNode* parse_function_call(Parser* parser) {
    // The identifier was already consumed, need to go back
    // This is a simplified version
    ASTNode* func = ast_new_node(NODE_FUNC_CALL, parser->previous.line, parser->previous.column);
    
    // Parse arguments
    parser_consume(parser, TK_LPAREN, "Expected '(' after function name");
    
    ASTNode* args = NULL;
    ASTNode** current_arg = &args;
    
    if (!parser_check(parser, TK_RPAREN)) {
        do {
            ASTNode* arg = parse_expression(parser);
            if (!arg) {
                ast_free(func);
                return NULL;
            }
            
            *current_arg = arg;
            current_arg = &((*current_arg)->right);
        } while (parser_match(parser, TK_COMMA));
    }
    
    parser_consume(parser, TK_RPAREN, "Expected ')' after arguments");
    
    // Store the function name (from previous token)
    func->left = ast_new_identifier(parser->previous.start, parser->previous.line, parser->previous.column);
    func->right = args;
    
    return func;
}

// Parse input statement
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

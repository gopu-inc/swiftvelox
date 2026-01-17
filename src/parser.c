#include "parser.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Variables globales du parser
Macro macros[MAX_MACROS];
int macro_count = 0;

ASTNode* new_node(NodeType type) {
    ASTNode* node = malloc(sizeof(ASTNode));
    node->type = type;
    node->token = current_token;
    node->left = node->right = NULL;
    node->children = NULL;
    node->child_count = 0;
    node->inferred_type = VAL_NIL;
    return node;
}

// Forward declarations pour éviter les warnings
ASTNode* expression();
ASTNode* statement();
ASTNode* declaration();
ASTNode* function_expression();
ASTNode* type_annotation();
ASTNode* pattern();
ASTNode* expression_statement();

// Primary expressions
ASTNode* primary() {
    if (match(TK_NUMBER)) {
        ASTNode* n = new_node(NODE_LITERAL);
        n->token = previous_token;
        return n;
    }
    if (match(TK_STRING_LIT) || match(TK_TEMPLATE_LIT)) {
        ASTNode* n = new_node(NODE_LITERAL);
        n->token = previous_token;
        return n;
    }
    if (match(TK_TRUE) || match(TK_FALSE) || match(TK_NIL) || match(TK_UNDEFINED)) {
        ASTNode* n = new_node(NODE_LITERAL);
        n->token = previous_token;
        return n;
    }
    if (match(TK_IDENTIFIER)) {
        ASTNode* n = new_node(NODE_IDENTIFIER);
        n->token = previous_token;
        return n;
    }
    if (match(TK_THIS)) {
        ASTNode* n = new_node(NODE_THIS);
        return n;
    }
    if (match(TK_SUPER)) {
        ASTNode* n = new_node(NODE_SUPER);
        consume(TK_DOT, "'.' attendu après 'super'");
        consume(TK_IDENTIFIER, "Nom de méthode attendu");
        n->token = previous_token;
        return n;
    }
    if (match(TK_LPAREN)) {
        ASTNode* expr = expression();
        consume(TK_RPAREN, "')' attendu");
        return expr;
    }
    if (match(TK_LBRACKET)) {
        ASTNode* n = new_node(NODE_ARRAY);
        if (!match(TK_RBRACKET)) {
            do {
                if (match(TK_DOTDOTDOT)) {
                    ASTNode* spread = new_node(NODE_SPREAD);
                    spread->left = expression();
                    n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                    n->children[n->child_count++] = spread;
                } else {
                    n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                    n->children[n->child_count++] = expression();
                }
            } while (match(TK_COMMA));
            consume(TK_RBRACKET, "']' attendu");
        }
        return n;
    }
    if (match(TK_LBRACE)) {
        ASTNode* n = new_node(NODE_OBJECT);
        if (!match(TK_RBRACE)) {
            do {
                ASTNode* prop = new_node(NODE_PROPERTY);
                
                if (match(TK_IDENTIFIER)) {
                    prop->token = previous_token;
                } else if (match(TK_STRING_LIT)) {
                    prop->token = previous_token;
                } else {
                    syntax_error(current_token, "Clé de propriété attendue");
                }
                
                consume(TK_COLON, "':' attendu");
                prop->left = expression();
                
                n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                n->children[n->child_count++] = prop;
            } while (match(TK_COMMA));
            consume(TK_RBRACE, "'}' attendu");
        }
        return n;
    }
    if (match(TK_FN)) {
        return function_expression();
    }
    if (match(TK_ASYNC)) {
        ASTNode* n = new_node(NODE_ASYNC);
        n->left = expression(); // Function expression
        return n;
    }
    
    syntax_error(current_token, "Expression attendue");
    return NULL;
}

// Call and member access
ASTNode* call() {
    ASTNode* expr = primary();
    
    while (1) {
        if (match(TK_LPAREN)) {
            ASTNode* n = new_node(NODE_CALL);
            n->left = expr;
            
            if (!match(TK_RPAREN)) {
                do {
                    n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                    n->children[n->child_count++] = expression();
                } while (match(TK_COMMA));
                consume(TK_RPAREN, "')' attendu");
            }
            expr = n;
        }
        else if (match(TK_DOT)) {
            ASTNode* n = new_node(NODE_ACCESS);
            n->left = expr;
            consume(TK_IDENTIFIER, "Propriété attendue");
            n->token = previous_token;
            expr = n;
        }
        else if (match(TK_LBRACKET)) {
            ASTNode* n = new_node(NODE_INDEX);
            n->left = expr;
            n->right = expression();
            consume(TK_RBRACKET, "']' attendu");
            expr = n;
        }
        else if (match(TK_QUESTION) && current_token.type == TK_DOT) {
            next_token(); // Skip .
            ASTNode* n = new_node(NODE_ACCESS);
            n->left = expr;
            consume(TK_IDENTIFIER, "Propriété attendue");
            n->token = previous_token;
            expr = n;
        }
        else {
            break;
        }
    }
    
    return expr;
}

// Unary operators
ASTNode* unary() {
    if (match(TK_BANG) || match(TK_MINUS) || match(TK_PLUS) || match(TK_TILDE) ||
        match(TK_TYPEOF) || match(TK_VOID) || match(TK_DELETE) || match(TK_AWAIT)) {
        ASTNode* n = new_node(NODE_UNARY);
        n->token = previous_token;
        n->right = unary();
        return n;
    }
    
    if (match(TK_PLUS_PLUS) || match(TK_MINUS_MINUS)) {
        ASTNode* n = new_node(NODE_UNARY);
        n->token = previous_token;
        n->right = call();
        return n;
    }
    
    return call();
}

// Multiplication and division
ASTNode* multiplication() {
    ASTNode* expr = unary();
    
    while (match(TK_STAR) || match(TK_SLASH) || match(TK_PERCENT)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = unary();
        expr = n;
    }
    
    return expr;
}

// Addition and subtraction
ASTNode* addition() {
    ASTNode* expr = multiplication();
    
    while (match(TK_PLUS) || match(TK_MINUS)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = multiplication();
        expr = n;
    }
    
    return expr;
}

// Shift operators
ASTNode* shift() {
    ASTNode* expr = addition();
    
    while (match(TK_LSHIFT) || match(TK_RSHIFT) || match(TK_URSHIFT)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = addition();
        expr = n;
    }
    
    return expr;
}

// Relational operators
ASTNode* relational() {
    ASTNode* expr = shift();
    
    while (match(TK_LT) || match(TK_GT) || match(TK_LTEQ) || match(TK_GTEQ) ||
           match(TK_INSTANCEOF) || match(TK_IN)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = shift();
        expr = n;
    }
    
    return expr;
}

// Equality operators
ASTNode* equality() {
    ASTNode* expr = relational();
    
    while (match(TK_EQEQ) || match(TK_BANGEQ) || match(TK_EQEQEQ) || match(TK_BANGEQEQ)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = relational();
        expr = n;
    }
    
    return expr;
}

// Bitwise AND
ASTNode* bitwise_and() {
    ASTNode* expr = equality();
    
    while (match(TK_AMPERSAND)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = equality();
        expr = n;
    }
    
    return expr;
}

// Bitwise XOR
ASTNode* bitwise_xor() {
    ASTNode* expr = bitwise_and();
    
    while (match(TK_CARET)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = bitwise_and();
        expr = n;
    }
    
    return expr;
}

// Bitwise OR
ASTNode* bitwise_or() {
    ASTNode* expr = bitwise_xor();
    
    while (match(TK_BAR)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = bitwise_xor();
        expr = n;
    }
    
    return expr;
}

// Logical AND
ASTNode* logical_and() {
    ASTNode* expr = bitwise_or();
    
    while (match(TK_AND)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = bitwise_or();
        expr = n;
    }
    
    return expr;
}

// Logical OR
ASTNode* logical_or() {
    ASTNode* expr = logical_and();
    
    while (match(TK_OR)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = logical_and();
        expr = n;
    }
    
    return expr;
}

// Nullish coalescing
ASTNode* nullish_coalescing() {
    ASTNode* expr = logical_or();
    
    while (match(TK_DOUBLE_QUESTION)) {
        ASTNode* n = new_node(NODE_BINARY);
        n->token = previous_token;
        n->left = expr;
        n->right = logical_or();
        expr = n;
    }
    
    return expr;
}

// Ternary conditional
ASTNode* conditional() {
    ASTNode* expr = nullish_coalescing();
    
    if (match(TK_QUESTION)) {
        ASTNode* n = new_node(NODE_TERNARY);
        n->left = expr; // Condition
        n->right = expression(); // Then
        consume(TK_COLON, "':' attendu");
        ASTNode* else_node = new_node(NODE_TERNARY);
        else_node->left = expression(); // Else
        n->children = malloc(sizeof(ASTNode*));
        n->children[0] = else_node;
        n->child_count = 1;
        return n;
    }
    
    return expr;
}

// Pipeline operator
ASTNode* pipeline() {
    ASTNode* expr = conditional();
    
    while (match(TK_PIPELINE)) {
        ASTNode* n = new_node(NODE_PIPELINE);
        n->left = expr;
        n->right = conditional();
        expr = n;
    }
    
    return expr;
}

// Assignment
ASTNode* assignment() {
    ASTNode* expr = pipeline();
    
    if (match(TK_EQ) || match(TK_PLUS_EQ) || match(TK_MINUS_EQ) || 
        match(TK_STAR_EQ) || match(TK_SLASH_EQ) || match(TK_PERCENT) ||
        match(TK_LSHIFT_EQ) || match(TK_RSHIFT_EQ) || match(TK_URSHIFT_EQ) ||
        match(TK_AMPERSAND_EQ) || match(TK_BAR_EQ) || match(TK_CARET_EQ) ||
        match(TK_PLUS_EQ_EQ) || match(TK_MINUS_EQ_EQ)) {
        
        ASTNode* n;
        if (previous_token.type == TK_EQ) {
            n = new_node(NODE_ASSIGN);
        } else {
            n = new_node(NODE_COMPOUND_ASSIGN);
        }
        n->token = previous_token;
        n->left = expr;
        n->right = assignment();
        return n;
    }
    
    return expr;
}

// Expression
ASTNode* expression() {
    return assignment();
}

// Function expression
ASTNode* function_expression() {
    ASTNode* n = new_node(NODE_FUNCTION);
    
    // Optional name
    if (current_token.type == TK_IDENTIFIER) {
        n->token = previous_token;
        next_token();
    }
    
    consume(TK_LPAREN, "'(' attendu");
    
    // Parameters
    if (!match(TK_RPAREN)) {
        do {
            // Rest parameter
            if (match(TK_DOTDOTDOT)) {
                ASTNode* rest = new_node(NODE_REST);
                consume(TK_IDENTIFIER, "Nom de paramètre attendu");
                rest->token = previous_token;
                n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
                n->children[n->child_count++] = rest;
                break;
            }
            
            ASTNode* param = new_node(NODE_IDENTIFIER);
            consume(TK_IDENTIFIER, "Nom de paramètre attendu");
            param->token = previous_token;
            
            // Optional type annotation
            if (match(TK_COLON)) {
                ASTNode* type = type_annotation();
                param->left = type;
            }
            
            // Optional default value
            if (match(TK_EQ)) {
                param->right = expression();
            }
            
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = param;
        } while (match(TK_COMMA));
        consume(TK_RPAREN, "')' attendu");
    }
    
    // Return type annotation
    if (match(TK_COLON)) {
        ASTNode* return_type = type_annotation();
        n->right = return_type;
    }
    
    // Arrow function or block
    if (match(TK_ARROW) || match(TK_FAT_ARROW)) {
        ASTNode* body;
        if (current_token.type == TK_LBRACE) {
            body = statement(); // Block
        } else {
            body = expression(); // Single expression
        }
        n->left = body;
    } else {
        n->left = statement(); // Block
    }
    
    return n;
}

// Type annotation
ASTNode* type_annotation() {
    // Simple type system for now
    ASTNode* n = new_node(NODE_TYPE_DECL);
    
    if (match(TK_IDENTIFIER)) {
        n->token = previous_token;
    } else if (match(TK_STRING_TYPE) || match(TK_INT_TYPE) || 
               match(TK_FLOAT_TYPE) || match(TK_BOOL_TYPE) ||
               match(TK_VOID_TYPE) || match(TK_ANY_TYPE)) {
        n->token = previous_token;
    } else if (match(TK_LBRACKET)) {
        n->token.type = TK_ARRAY_TYPE;
        n->left = type_annotation();
        consume(TK_RBRACKET, "']' attendu");
    } else {
        syntax_error(current_token, "Type attendu");
    }
    
    // Optional nullable
    if (match(TK_QUESTION)) {
        ASTNode* nullable = new_node(NODE_NULLABLE_TYPE);
        nullable->left = n;
        return nullable;
    }
    
    return n;
}

// Expression statement
ASTNode* expression_statement() {
    ASTNode* expr = expression();
    consume(TK_SEMICOLON, "';' attendu");
    
    ASTNode* n = new_node(NODE_EXPR_STMT);
    n->left = expr;
    return n;
}

// Variable declaration
ASTNode* var_declaration() {
    TokenType decl_type = current_token.type;
    next_token(); // Skip let/const/var/mut
    
    ASTNode* n;
    if (decl_type == TK_LET || decl_type == TK_CONST) {
        n = new_node(decl_type == TK_LET ? NODE_VAR_DECL : NODE_CONST_DECL);
    } else {
        n = new_node(NODE_VAR_DECL);
    }
    
    consume(TK_IDENTIFIER, "Nom de variable attendu");
    n->token = previous_token;
    
    // Type annotation
    if (match(TK_COLON)) {
        ASTNode* type = type_annotation();
        n->left = type;
    }
    
    // Initializer
    if (match(TK_EQ)) {
        n->right = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Block statement
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

// If statement
ASTNode* if_statement() {
    ASTNode* n = new_node(NODE_IF);
    
    consume(TK_LPAREN, "'(' attendu après 'if'");
    n->left = expression(); // Condition
    consume(TK_RPAREN, "')' attendu");
    
    n->children = malloc(sizeof(ASTNode*) * 3);
    n->child_count = 0;
    
    // Then branch
    n->children[n->child_count++] = statement();
    
    // Elif branches
    while (match(TK_ELIF)) {
        ASTNode* elif = new_node(NODE_ELIF);
        consume(TK_LPAREN, "'(' attendu après 'elif'");
        elif->left = expression();
        consume(TK_RPAREN, "')' attendu");
        elif->right = statement();
        
        n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
        n->children[n->child_count++] = elif;
    }
    
    // Else branch
    if (match(TK_ELSE)) {
        n->children[n->child_count++] = statement();
    }
    
    return n;
}

// While statement
ASTNode* while_statement() {
    ASTNode* n = new_node(NODE_WHILE);
    
    consume(TK_LPAREN, "'(' attendu après 'while'");
    n->left = expression();
    consume(TK_RPAREN, "')' attendu");
    
    n->right = statement();
    return n;
}

// Return statement
ASTNode* return_statement() {
    ASTNode* n = new_node(NODE_RETURN);
    
    if (current_token.type != TK_SEMICOLON) {
        n->left = expression();
    }
    
    consume(TK_SEMICOLON, "';' attendu");
    return n;
}

// Function declaration
ASTNode* function_declaration() {
    ASTNode* n = new_node(NODE_FUNCTION);
    
    consume(TK_IDENTIFIER, "Nom de fonction attendu");
    n->token = previous_token;
    
    consume(TK_LPAREN, "'(' attendu");
    
    // Parameters
    if (!match(TK_RPAREN)) {
        do {
            ASTNode* param = new_node(NODE_IDENTIFIER);
            consume(TK_IDENTIFIER, "Nom de paramètre attendu");
            param->token = previous_token;
            
            // Type annotation
            if (match(TK_COLON)) {
                param->left = type_annotation();
            }
            
            // Default value
            if (match(TK_EQ)) {
                param->right = expression();
            }
            
            n->children = realloc(n->children, sizeof(ASTNode*) * (n->child_count + 1));
            n->children[n->child_count++] = param;
        } while (match(TK_COMMA));
        consume(TK_RPAREN, "')' attendu");
    }
    
    // Return type
    if (match(TK_COLON)) {
        n->right = type_annotation();
    }
    
    n->left = statement(); // Function body
    
    return n;
}

// Statement
ASTNode* statement() {
    if (match(TK_IF)) return if_statement();
    if (match(TK_WHILE)) return while_statement();
    if (match(TK_RETURN)) return return_statement();
    if (match(TK_BREAK)) {
        ASTNode* n = new_node(NODE_BREAK);
        consume(TK_SEMICOLON, "';' attendu");
        return n;
    }
    if (match(TK_CONTINUE)) {
        ASTNode* n = new_node(NODE_CONTINUE);
        consume(TK_SEMICOLON, "';' attendu");
        return n;
    }
    if (match(TK_LBRACE)) return block_statement();
    
    return expression_statement();
}

// Declaration (top-level)
ASTNode* declaration() {
    if (match(TK_LET) || match(TK_CONST) || match(TK_VAR) || match(TK_MUT)) {
        return var_declaration();
    }
    if (match(TK_FN)) {
        return function_declaration();
    }
    
    return statement();
}

// Parse program
ASTNode* parse(const char* source) {
    init_scanner(source);
    next_token();
    
    ASTNode* program = new_node(NODE_PROGRAM);
    
    while (!is_at_end() && current_token.type != TK_EOF) {
        program->children = realloc(program->children, sizeof(ASTNode*) * (program->child_count + 1));
        program->children[program->child_count++] = declaration();
    }
    
    return program;
}

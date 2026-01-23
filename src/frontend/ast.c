#include "ast.h"
#include "common.h"
#include <stdio.h>

// Create a new AST node
ASTNode* ast_new_node(NodeType type, int line, int column) {
    ASTNode* node = ALLOC(ASTNode);
    CHECK_ALLOC(node, "AST node allocation");
    
    node->type = type;
    node->left = node->right = node->third = node->fourth = NULL;
    node->line = line;
    node->column = column;
    node->op_type = TK_ERROR;
    
    return node;
}

// Create literal nodes
ASTNode* ast_new_int(int64_t value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_INT, line, column);
    node->data.int_val = value;
    return node;
}

ASTNode* ast_new_float(double value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_FLOAT, line, column);
    node->data.float_val = value;
    return node;
}

ASTNode* ast_new_string(const char* value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_STRING, line, column);
    node->data.str_val = str_copy(value);
    return node;
}

ASTNode* ast_new_bool(bool value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_BOOL, line, column);
    node->data.bool_val = value;
    return node;
}

ASTNode* ast_new_identifier(const char* name, int line, int column) {
    ASTNode* node = ast_new_node(NODE_IDENT, line, column);
    node->data.name = str_copy(name);
    return node;
}

// Create operation nodes
ASTNode* ast_new_binary(NodeType type, ASTNode* left, ASTNode* right, int line, int column) {
    ASTNode* node = ast_new_node(type, line, column);
    node->left = left;
    node->right = right;
    return node;
}

ASTNode* ast_new_unary(NodeType type, ASTNode* operand, int line, int column) {
    ASTNode* node = ast_new_node(type, line, column);
    node->left = operand;
    return node;
}

ASTNode* ast_new_assignment(ASTNode* left, ASTNode* right, int line, int column) {
    ASTNode* node = ast_new_node(NODE_ASSIGN, line, column);
    node->left = left;
    node->right = right;
    return node;
}

// Create variable declaration
ASTNode* ast_new_var_decl(const char* name, ASTNode* value, TokenKind var_type, int line, int column) {
    ASTNode* node = NULL;
    
    switch (var_type) {
        case TK_VAR: node = ast_new_node(NODE_VAR_DECL, line, column); break;
        case TK_NET: node = ast_new_node(NODE_NET_DECL, line, column); break;
        case TK_CLOG: node = ast_new_node(NODE_CLOG_DECL, line, column); break;
        case TK_DOS: node = ast_new_node(NODE_DOS_DECL, line, column); break;
        case TK_SEL: node = ast_new_node(NODE_SEL_DECL, line, column); break;
        case TK_CONST: node = ast_new_node(NODE_CONST_DECL, line, column); break;
        case TK_GLOBAL: node = ast_new_node(NODE_GLOBAL_DECL, line, column); break;
        default: node = ast_new_node(NODE_VAR_DECL, line, column); break;
    }
    
    node->data.name = str_copy(name);
    node->left = value; // Initial value
    node->op_type = var_type;
    
    return node;
}

// Create control flow nodes
ASTNode* ast_new_if(ASTNode* condition, ASTNode* then_branch, ASTNode* else_branch, int line, int column) {
    ASTNode* node = ast_new_node(NODE_IF, line, column);
    node->left = condition;
    node->right = then_branch;
    node->third = else_branch;
    return node;
}

ASTNode* ast_new_while(ASTNode* condition, ASTNode* body, int line, int column) {
    ASTNode* node = ast_new_node(NODE_WHILE, line, column);
    node->left = condition;
    node->right = body;
    return node;
}

ASTNode* ast_new_for(ASTNode* init, ASTNode* condition, ASTNode* update, ASTNode* body, int line, int column) {
    ASTNode* node = ast_new_node(NODE_FOR, line, column);
    node->data.loop.init = init;
    node->data.loop.condition = condition;
    node->data.loop.update = update;
    node->data.loop.body = body;
    return node;
}

// Create function node (simplifié)
ASTNode* ast_new_function(const char* name, ASTNode* params, ASTNode* body, int line, int column) {
    ASTNode* node = ast_new_node(NODE_FUNC, line, column);
    node->data.func_def.name = str_copy(name);
    node->data.func_def.params = params;
    node->data.func_def.body = body;
    return node;
}

// Create function call node
ASTNode* ast_new_function_call(ASTNode* function, ASTNode* args, int line, int column) {
    ASTNode* node = ast_new_node(NODE_FUNC_CALL, line, column);
    node->data.func_call.function = function;
    node->data.func_call.arguments = args;
    
    // Count arguments
    int count = 0;
    ASTNode* arg = args;
    while (arg) {
        count++;
        arg = arg->right;
    }
    node->data.func_call.arg_count = count;
    
    return node;
}

// Create return node
ASTNode* ast_new_return(ASTNode* value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_RETURN, line, column);
    node->left = value;
    return node;
}

// Create import node (simplifié)
ASTNode* ast_new_import(char** modules, int count, const char* from_module, int line, int column) {
    ASTNode* node = ast_new_node(NODE_IMPORT, line, column);
    
    // Prend seulement le premier module pour simplifier
    if (count > 0 && modules[0]) {
        node->data.import_info.module_name = str_copy(modules[0]);
    } else {
        node->data.import_info.module_name = NULL;
    }
    
    node->data.import_info.from_module = from_module ? str_copy(from_module) : NULL;
    
    // Libérer le tableau de modules
    if (modules) {
        for (int i = 0; i < count; i++) {
            FREE(modules[i]);
        }
        FREE(modules);
    }
    
    return node;
}

// Create print node
ASTNode* ast_new_print(ASTNode* value, int line, int column) {
    ASTNode* node = ast_new_node(NODE_PRINT, line, column);
    node->left = value;
    return node;
}

// Create input node
ASTNode* ast_new_input(const char* prompt, int line, int column) {
    ASTNode* node = ast_new_node(NODE_INPUT, line, column);
    node->data.input_op.prompt = prompt ? str_copy(prompt) : NULL;
    return node;
}

// Free AST node recursively
void ast_free(ASTNode* node) {
    if (!node) return;
    
    // Free children
    ast_free(node->left);
    ast_free(node->right);
    ast_free(node->third);
    ast_free(node->fourth);
    
    // Free node-specific data
    switch (node->type) {
        case NODE_STRING:
        case NODE_IDENT:
            FREE(node->data.str_val);
            break;
        case NODE_FUNC:
            FREE(node->data.func_def.name);
            ast_free(node->data.func_def.params);
            ast_free(node->data.func_def.body);
            break;
        case NODE_IMPORT:
            FREE(node->data.import_info.module_name);
            FREE(node->data.import_info.from_module);
            break;
        case NODE_INPUT:
            FREE(node->data.input_op.prompt);
            break;
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL:
        case NODE_GLOBAL_DECL:
            FREE(node->data.name);
            break;
        default:
            // No special cleanup needed
            break;
    }
    
    FREE(node);
}

// Print AST for debugging
void ast_print(ASTNode* node, int indent) {
    if (!node) return;
    
    for (int i = 0; i < indent; i++) printf("  ");
    
    printf("%s (%d:%d)", node_type_to_string(node->type), node->line, node->column);
    
    switch (node->type) {
        case NODE_INT:
            printf(": %lld\n", (long long)node->data.int_val);
            break;
        case NODE_FLOAT:
            printf(": %f\n", node->data.float_val);
            break;
        case NODE_STRING:
            printf(": \"%s\"\n", node->data.str_val);
            break;
        case NODE_BOOL:
            printf(": %s\n", node->data.bool_val ? "true" : "false");
            break;
        case NODE_IDENT:
            printf(": %s\n", node->data.name);
            break;
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL:
        case NODE_GLOBAL_DECL:
            printf(": %s\n", node->data.name);
            ast_print(node->left, indent + 1);
            break;
        case NODE_FUNC:
            printf(": %s\n", node->data.func_def.name);
            ast_print(node->data.func_def.params, indent + 1);
            ast_print(node->data.func_def.body, indent + 1);
            break;
        case NODE_IMPORT:
            printf(": import ");
            if (node->data.import_info.module_name) {
                printf("%s", node->data.import_info.module_name);
            }
            if (node->data.import_info.from_module) {
                printf(" from %s", node->data.import_info.from_module);
            }
            printf("\n");
            break;
        case NODE_IF:
            printf("\n");
            ast_print(node->left, indent + 1);  // Condition
            printf("%*sThen:\n", indent * 2, "");
            ast_print(node->right, indent + 1); // Then branch
            if (node->third) {
                printf("%*sElse:\n", indent * 2, "");
                ast_print(node->third, indent + 1); // Else branch
            }
            break;
        default:
            printf("\n");
            ast_print(node->left, indent + 1);
            ast_print(node->right, indent + 1);
            ast_print(node->third, indent + 1);
            ast_print(node->fourth, indent + 1);
            break;
    }
}

// Convert node type to string
const char* node_type_to_string(NodeType type) {
    switch (type) {
        case NODE_INT: return "INT";
        case NODE_FLOAT: return "FLOAT";
        case NODE_STRING: return "STRING";
        case NODE_BOOL: return "BOOL";
        case NODE_IDENT: return "IDENT";
        case NODE_NULL: return "NULL";
        case NODE_UNDEFINED: return "UNDEFINED";
        case NODE_NAN: return "NAN";
        case NODE_INF: return "INF";
        case NODE_LIST: return "LIST";
        case NODE_MAP: return "MAP";
        case NODE_FUNC: return "FUNC";
        case NODE_FUNC_CALL: return "FUNC_CALL";
        case NODE_LAMBDA: return "LAMBDA";
        case NODE_ARRAY_ACCESS: return "ARRAY_ACCESS";
        case NODE_MEMBER_ACCESS: return "MEMBER_ACCESS";
        case NODE_BINARY: return "BINARY";
        case NODE_UNARY: return "UNARY";
        case NODE_TERNARY: return "TERNARY";
        case NODE_ASSIGN: return "ASSIGN";
        case NODE_COMPOUND_ASSIGN: return "COMPOUND_ASSIGN";
        case NODE_IF: return "IF";
        case NODE_WHILE: return "WHILE";
        case NODE_FOR: return "FOR";
        case NODE_FOR_IN: return "FOR_IN";
        case NODE_SWITCH: return "SWITCH";
        case NODE_CASE: return "CASE";
        case NODE_RETURN: return "RETURN";
        case NODE_YIELD: return "YIELD";
        case NODE_BREAK: return "BREAK";
        case NODE_CONTINUE: return "CONTINUE";
        case NODE_THROW: return "THROW";
        case NODE_TRY: return "TRY";
        case NODE_CATCH: return "CATCH";
        case NODE_VAR_DECL: return "VAR_DECL";
        case NODE_NET_DECL: return "NET_DECL";
        case NODE_CLOG_DECL: return "CLOG_DECL";
        case NODE_DOS_DECL: return "DOS_DECL";
        case NODE_SEL_DECL: return "SEL_DECL";
        case NODE_CONST_DECL: return "CONST_DECL";
        case NODE_GLOBAL_DECL: return "GLOBAL_DECL";
        case NODE_SIZEOF: return "SIZEOF";
        case NODE_NEW: return "NEW";
        case NODE_DELETE: return "DELETE";
        case NODE_FREE: return "FREE";
        case NODE_IMPORT: return "IMPORT";
        case NODE_EXPORT: return "EXPORT";
        case NODE_MODULE: return "MODULE";
        case NODE_DBVAR: return "DBVAR";
        case NODE_ASSERT: return "ASSERT";
        case NODE_PRINT: return "PRINT";
        case NODE_WELD: return "WELD";
        case NODE_READ: return "READ";
        case NODE_WRITE: return "WRITE";
        case NODE_INPUT: return "INPUT";
        case NODE_PASS: return "PASS";
        case NODE_WITH: return "WITH";
        case NODE_LEARN: return "LEARN";
        case NODE_LOCK: return "LOCK";
        case NODE_APPEND: return "APPEND";
        case NODE_PUSH: return "PUSH";
        case NODE_POP: return "POP";
        case NODE_CLASS: return "CLASS";
        case NODE_STRUCT: return "STRUCT";
        case NODE_ENUM: return "ENUM";
        case NODE_INTERFACE: return "INTERFACE";
        case NODE_TYPEDEF: return "TYPEDEF";
        case NODE_NAMESPACE: return "NAMESPACE";
        case NODE_NEW_INSTANCE: return "NEW_INSTANCE";
        case NODE_METHOD_CALL: return "METHOD_CALL";
        case NODE_PROPERTY_ACCESS: return "PROPERTY_ACCESS";
        case NODE_JSON: return "JSON";
        case NODE_YAML: return "YAML";
        case NODE_XML: return "XML";
        case NODE_ASYNC: return "ASYNC";
        case NODE_AWAIT: return "AWAIT";
        case NODE_BLOCK: return "BLOCK";
        case NODE_SCOPE: return "SCOPE";
        case NODE_MAIN: return "MAIN";
        case NODE_PROGRAM: return "PROGRAM";
        case NODE_EMPTY: return "EMPTY";
        default: return "UNKNOWN";
    }
}

// Convert token kind to string
const char* token_kind_to_string(TokenKind kind) {
    // Tableau pour une recherche rapide
    static const char* token_names[] = {
        "INT", "FLOAT", "STRING", "TRUE", "FALSE",
        "NULL", "UNDEFINED", "NAN", "INF",
        "IDENT", "AS", "OF",
        "PLUS", "MINUS", "MULT", "DIV", "MOD",
        "POW", "CONCAT", "SPREAD", "NULLISH",
        "ASSIGN", "EQ", "NEQ", "GT", "LT", "GTE", "LTE",
        "PLUS_ASSIGN", "MINUS_ASSIGN", "MULT_ASSIGN", 
        "DIV_ASSIGN", "MOD_ASSIGN", "POW_ASSIGN",
        "CONCAT_ASSIGN",
        "AND", "OR", "NOT",
        "BIT_AND", "BIT_OR", "BIT_XOR", "BIT_NOT",
        "SHL", "SHR", "USHR",
        "RARROW", "DARROW", "LDARROW", "RDARROW",
        "SPACESHIP", "ELLIPSIS", "RANGE", "RANGE_INCL",
        "QUESTION", "SCOPE", "SAFE_NAV",
        "IN", "IS", "ISNOT", "AS_OP",
        "LPAREN", "RPAREN", "LBRACE", "RBRACE",
        "LBRACKET", "RBRACKET",
        "COMMA", "SEMICOLON", "COLON", "PERIOD",
        "AT", "HASH", "DOLLAR", "BACKTICK",
        "PIPE", "AMPERSAND", "CARET", "TILDE",
        "EXCLAMATION", "QUESTION_MARK",
        "LSQUARE", "RSQUARE",
        "VAR", "LET", "CONST",
        "NET", "CLOG", "DOS", "SEL",
        "THEN", "DO",
        "IF", "ELSE", "ELIF",
        "WHILE", "FOR", "SWITCH", "CASE", "DEFAULT",
        "BREAK", "CONTINUE", "RETURN", "YIELD",
        "TRY", "CATCH", "FINALLY", "THROW",
        "FUNC", "IMPORT", "EXPORT", "FROM",
        "CLASS", "STRUCT", "ENUM", "INTERFACE",
        "TYPEDEF", "TYPELOCK", "NAMESPACE",
        "TYPE_INT", "TYPE_FLOAT", "TYPE_STR",
        "TYPE_BOOL", "TYPE_CHAR", "TYPE_VOID",
        "TYPE_ANY", "TYPE_AUTO", "TYPE_UNKNOWN",
        "TYPE_NET", "TYPE_CLOG", "TYPE_DOS",
        "TYPE_SEL", "TYPE_ARRAY", "TYPE_MAP",
        "TYPE_FUNC", "DECREMENT", "INCREMENT", "TYPEOF",
        "SIZEOF", "SIZE", "SIZ",
        "NEW", "DELETE", "FREE",
        "DB", "DBVAR", "PRINT_DB", "ASSERT",
        "PRINT", "WELD", "READ", "WRITE", "INPUT",
        "PASS", "GLOBAL", "LAMBDA",
        "BDD", "DEF", "TYPE", "RAISE",
        "WITH", "LEARN", "NONLOCAL", "LOCK", 
        "APPEND", "PUSH", "POP", "TO",
        "JSON", "YAML", "XML",
        "MAIN", "THIS", "SELF", "SUPER",
        "STATIC", "PUBLIC", "PRIVATE", "PROTECTED",
        "ASYNC", "AWAIT",
        "FILE_OPEN", "FILE_CLOSE", "FILE_READ", "FILE_WRITE",
        "EOF", "ERROR"
    };
    
    if (kind >= 0 && kind < sizeof(token_names) / sizeof(token_names[0])) {
        return token_names[kind];
    }
    return "UNKNOWN_TOKEN";
}

// AST Optimizer (simple version)
ASTNode* ast_optimize(ASTNode* node) {
    if (!node) return NULL;
    
    // Optimize children first
    node->left = ast_optimize(node->left);
    node->right = ast_optimize(node->right);
    node->third = ast_optimize(node->third);
    node->fourth = ast_optimize(node->fourth);
    
    // Constant folding for binary operations
    if (node->type == NODE_BINARY) {
        if (node->left && node->right) {
            // Check if both operands are constants
            if (node->left->type == NODE_INT && node->right->type == NODE_INT) {
                int64_t left_val = node->left->data.int_val;
                int64_t right_val = node->right->data.int_val;
                int64_t result = 0;
                
                switch (node->op_type) {
                    case TK_PLUS:
                        result = left_val + right_val;
                        ast_free(node->left);
                        ast_free(node->right);
                        node->type = NODE_INT;
                        node->data.int_val = result;
                        node->left = node->right = NULL;
                        break;
                    case TK_MINUS:
                        result = left_val - right_val;
                        ast_free(node->left);
                        ast_free(node->right);
                        node->type = NODE_INT;
                        node->data.int_val = result;
                        node->left = node->right = NULL;
                        break;
                    case TK_MULT:
                        result = left_val * right_val;
                        ast_free(node->left);
                        ast_free(node->right);
                        node->type = NODE_INT;
                        node->data.int_val = result;
                        node->left = node->right = NULL;
                        break;
                    case TK_DIV:
                        if (right_val != 0) {
                            result = left_val / right_val;
                            ast_free(node->left);
                            ast_free(node->right);
                            node->type = NODE_INT;
                            node->data.int_val = result;
                            node->left = node->right = NULL;
                        }
                        break;
                    default:
                        // Leave other operators unchanged
                        break;
                }
            }
        }
    }
    
    return node;
}

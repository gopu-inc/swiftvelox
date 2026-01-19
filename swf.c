#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// ======================================================
// [SECTION] VM STATE - SIMPLIFIÉ
// ======================================================
typedef struct {
    char name[100];
    int size_bytes;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
    } value;
    bool is_float;
    bool is_initialized;
    int scope_level;
} Variable;

static Variable vars[100];
static int var_count = 0;
static int scope_level = 0;

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static int calculateVariableSize(TokenKind type) {
    switch (type) {
        case TK_VAR: return (rand() % 5) + 1; // 1-5 bytes
        case TK_NET: return (rand() % 8) + 1; // 1-8 bytes
        case TK_CLOG: return (rand() % 25) + 1; // 1-25 bytes
        case TK_DOS: return (rand() % 1024) + 1; // 1-1024 bytes
        default: return 4;
    }
}

static const char* getTypeName(TokenKind type) {
    switch (type) {
        case TK_VAR: return "var";
        case TK_NET: return "net";
        case TK_CLOG: return "clog";
        case TK_DOS: return "dos";
        case TK_SEL: return "sel";
        default: return "unknown";
    }
}

static int findVar(const char* name) {
    for (int i = var_count - 1; i >= 0; i--) {
        if (strcmp(vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

// ======================================================
// [SECTION] EXPRESSION EVALUATION
// ======================================================
static double evalFloat(ASTNode* node) {
    if (!node) return 0.0;
    
    switch (node->type) {
        case NODE_INT:
            return (double)node->data.int_val;
            
        case NODE_FLOAT:
            return node->data.float_val;
            
        case NODE_BOOL:
            return node->data.bool_val ? 1.0 : 0.0;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_float) {
                    return vars[idx].value.float_val;
                } else {
                    return (double)vars[idx].value.int_val;
                }
            }
            printf(RED "[ERROR]" RESET " Undefined variable: %s\n", node->data.name);
            return 0.0;
        }
            
        case NODE_BINARY: {
            double left = evalFloat(node->left);
            double right = evalFloat(node->right);
            
            switch (node->op_type) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: return right != 0 ? left / right : 0;
                case TK_MOD: return right != 0 ? fmod(left, right) : 0;
                default: return 0.0;
            }
        }
            
        default:
            return 0.0;
    }
}

static char* evalString(ASTNode* node) {
    if (!node) return str_copy("");
    
    char* result = NULL;
    
    switch (node->type) {
        case NODE_STRING:
            result = str_copy(node->data.str_val);
            break;
            
        case NODE_INT: {
            result = malloc(32);
            sprintf(result, "%lld", node->data.int_val);
            break;
        }
            
        case NODE_FLOAT: {
            result = malloc(32);
            sprintf(result, "%.2f", node->data.float_val);
            break;
        }
            
        case NODE_BOOL:
            result = str_copy(node->data.bool_val ? "true" : "false");
            break;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_float) {
                    result = malloc(32);
                    sprintf(result, "%.2f", vars[idx].value.float_val);
                } else {
                    result = malloc(32);
                    sprintf(result, "%lld", vars[idx].value.int_val);
                }
            } else {
                result = str_copy("undefined");
            }
            break;
        }
            
        default:
            result = str_copy("");
            break;
    }
    
    return result;
}

// ======================================================
// [SECTION] EXECUTION
// ======================================================
static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL: {
            TokenKind var_type = TK_VAR;
            if (node->type == NODE_NET_DECL) var_type = TK_NET;
            else if (node->type == NODE_CLOG_DECL) var_type = TK_CLOG;
            else if (node->type == NODE_DOS_DECL) var_type = TK_DOS;
            else if (node->type == NODE_SEL_DECL) var_type = TK_SEL;
            else if (node->type == NODE_CONST_DECL) var_type = TK_CONST;
            
            if (var_count < 100) {
                Variable* var = &vars[var_count];
                strncpy(var->name, node->data.name, 99);
                var->name[99] = '\0';
                var->size_bytes = calculateVariableSize(var_type);
                var->scope_level = scope_level;
                
                if (node->left) {
                    var->is_initialized = true;
                    double val = evalFloat(node->left);
                    
                    if (node->left->type == NODE_FLOAT) {
                        var->is_float = true;
                        var->value.float_val = val;
                    } else {
                        var->is_float = false;
                        var->value.int_val = (int64_t)val;
                    }
                    
                    printf(GREEN "[DECL]" RESET " %s %s = ", 
                           getTypeName(var_type), var->name);
                    if (var->is_float) {
                        printf("%.2f", var->value.float_val);
                    } else {
                        printf("%lld", var->value.int_val);
                    }
                    printf(" (%d bytes)\n", var->size_bytes);
                } else {
                    var->is_initialized = false;
                    var->is_float = false;
                    var->value.int_val = 0;
                    
                    printf(GREEN "[DECL]" RESET " %s %s (uninitialized, %d bytes)\n", 
                           getTypeName(var_type), var->name, var->size_bytes);
                }
                
                var_count++;
            }
            break;
        }
            
        case NODE_PRINT: {
            if (node->left) {
                if (node->left->type == NODE_BINARY && node->left->op_type == TK_CONCAT) {
                    // Gestion de la concaténation
                    char* left_str = evalString(node->left->left);
                    char* right_str = evalString(node->left->right);
                    printf("%s%s", left_str, right_str);
                    free(left_str);
                    free(right_str);
                } else {
                    char* str = evalString(node->left);
                    printf("%s", str);
                    free(str);
                }
            }
            printf("\n");
            break;
        }
            
        case NODE_SIZEOF: {
            int idx = findVar(node->data.size_info.var_name);
            if (idx >= 0) {
                printf("%d bytes", vars[idx].size_bytes);
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", 
                       node->data.size_info.var_name);
            }
            break;
        }
            
        case NODE_DBVAR: {
            printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗\n");
            printf("║                    TABLE DES VARIABLES                    ║\n");
            printf("╠══════════════════════════════════════════════════════════════╣\n");
            printf("║  Type   │ Nom         │ Taille     │ Valeur       │ Init   ║\n");
            printf("╠══════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < var_count; i++) {
                Variable* var = &vars[i];
                const char* type_str = getTypeName(TK_VAR); // Simplifié
                
                printf("║ %-7s │ %-11s │ %-10d │ ", type_str, var->name, var->size_bytes);
                
                if (var->is_initialized) {
                    if (var->is_float) {
                        printf("%-12.2f │ %-6s ║\n", var->value.float_val, "✓");
                    } else {
                        printf("%-12lld │ %-6s ║\n", var->value.int_val, "✓");
                    }
                } else {
                    printf("N/A         │ %-6s ║\n", "✗");
                }
            }
            
            printf("╚══════════════════════════════════════════════════════════════╝\n" RESET);
            break;
        }
            
        case NODE_IF: {
            double condition = evalFloat(node->left);
            if (condition != 0) {
                execute(node->right);
            } else if (node->third) {
                execute(node->third);
            }
            break;
        }
            
        case NODE_WHILE: {
            while (evalFloat(node->left) != 0) {
                execute(node->right);
            }
            break;
        }
            
        case NODE_BLOCK: {
            scope_level++;
            ASTNode* current = node->left;
            while (current) {
                execute(current);
                current = current->right;
            }
            scope_level--;
            break;
        }
            
        case NODE_MAIN: {
            printf(CYAN "\n[EXEC]" RESET " Starting main()...\n");
            if (node->left) execute(node->left);
            printf(CYAN "[EXEC]" RESET " main() finished\n");
            break;
        }
            
        default:
            // Ignorer les autres types de nœuds pour l'instant
            break;
    }
}

// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void run(const char* source) {
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (nodes) {
        // Chercher d'abord la fonction main
        ASTNode* main_node = NULL;
        for (int i = 0; i < count; i++) {
            if (nodes[i] && nodes[i]->type == NODE_MAIN) {
                main_node = nodes[i];
                break;
            }
        }
        
        if (main_node) {
            execute(main_node);
        } else {
            // Exécuter toutes les déclarations dans l'ordre
            for (int i = 0; i < count; i++) {
                if (nodes[i]) {
                    execute(nodes[i]);
                }
            }
        }
        
        // Cleanup
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                // Libérer la mémoire du nœud
                if (nodes[i]->data.name) free(nodes[i]->data.name);
                if (nodes[i]->data.str_val) free(nodes[i]->data.str_val);
                free(nodes[i]);
            }
        }
        free(nodes);
    }
}

// ======================================================
// [SECTION] REPL
// ======================================================
static void repl() {
    printf(GREEN "╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                   SwiftFlow v2.0 - REPL Mode                  ║\n");
    printf("║          Types CLAIR & SYM Fusion - Complete Language         ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n" RESET);
    printf("Commands: exit, dbvar, clear\n\n");
    
    char line[1024];
    while (1) {
        printf("swift> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0) break;
        if (strcmp(line, "clear") == 0) {
            system("clear");
            continue;
        }
        if (strcmp(line, "dbvar") == 0) {
            // Afficher la table des variables
            printf(CYAN "\n╔══════════════════════════════════════════════════════════════╗\n");
            printf("║                    TABLE DES VARIABLES                    ║\n");
            printf("╠══════════════════════════════════════════════════════════════╣\n");
            printf("║  Type   │ Nom         │ Taille     │ Valeur       │ Init   ║\n");
            printf("╠══════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < var_count; i++) {
                Variable* var = &vars[i];
                const char* type_str = getTypeName(TK_VAR);
                
                printf("║ %-7s │ %-11s │ %-10d │ ", type_str, var->name, var->size_bytes);
                
                if (var->is_initialized) {
                    if (var->is_float) {
                        printf("%-12.2f │ %-6s ║\n", var->value.float_val, "✓");
                    } else {
                        printf("%-12lld │ %-6s ║\n", var->value.int_val, "✓");
                    }
                } else {
                    printf("N/A         │ %-6s ║\n", "✗");
                }
            }
            
            printf("╚══════════════════════════════════════════════════════════════╝\n" RESET);
            continue;
        }
        
        run(line);
    }
}

// ======================================================
// [SECTION] MAIN
// ======================================================
int main(int argc, char* argv[]) {
    srand(time(NULL)); // Pour les tailles aléatoires
    
    if (argc < 2) {
        repl();
    } else {
        FILE* f = fopen(argv[1], "r");
        if (!f) {
            printf(RED "[ERROR]" RESET " Cannot open file: %s\n", argv[1]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        fread(source, 1, size, f);
        source[size] = '\0';
        fclose(f);
        
        printf(GREEN "[SUCCESS]" RESET ">> Executing %s\n\n", argv[1]);
        
        run(source);
        free(source);
    }
    
    return 0;
}

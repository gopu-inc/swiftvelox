#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// ======================================================
// [SECTION] COULEURS POUR LE TERMINAL
// ======================================================
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define RESET   "\033[0m"

// ======================================================
// [SECTION] VM STATE
// ======================================================
typedef struct {
    char name[50];
    union {
        int int_val;
        float float_val;
    } value;
    bool is_float;
} Variable;

typedef struct {
    Variable vars[100];
    int count;
    char* import_path;
} VM;

static VM vm = {0};

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static char* my_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* d = malloc(len + 1);
    if (d) strcpy(d, s);
    return d;
}

static void setImportPath(const char* path) {
    if (vm.import_path) free(vm.import_path);
    vm.import_path = my_strdup(path);
}

static char* findImportFile(const char* module, const char* from_module) {
    // Si chemin direct
    if (module[0] == '.' || module[0] == '/') {
        return my_strdup(module);
    }
    
    // Chemin par défaut
    const char* base_path = from_module ? from_module : "/usr/local/lib/swift/";
    
    // Essayer avec .swf
    char* path = malloc(strlen(base_path) + strlen(module) + 10);
    sprintf(path, "%s%s.swf", base_path, module);
    
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return path;
    }
    
    // Essayer sans extension
    sprintf(path, "%s%s", base_path, module);
    f = fopen(path, "r");
    if (f) {
        fclose(f);
        return path;
    }
    
    free(path);
    return NULL;
}

static void importModule(const char* module, const char* from_module);
static void run(const char* source, const char* filename);

static void importModule(const char* module, const char* from_module) {
    char* path = findImportFile(module, from_module ? my_strdup(from_module) : NULL);
    if (!path) {
        printf(RED "[ERROR]" RESET " Cannot find module: %s\n", module);
        return;
    }
    
    FILE* f = fopen(path, "r");
    if (!f) {
        printf(RED "[ERROR]" RESET " Cannot open: %s\n", path);
        free(path);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    // Sauvegarder ancien chemin
    char* old_path = vm.import_path ? my_strdup(vm.import_path) : NULL;
    
    // Définir nouveau chemin
    char* dir_path = my_strdup(path);
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        *last_slash = '\0';
        setImportPath(dir_path);
    }
    free(dir_path);
    
    // Exécuter
    run(source, module);
    
    // Restaurer
    if (old_path) {
        setImportPath(old_path);
        free(old_path);
    } else if (vm.import_path) {
        free(vm.import_path);
        vm.import_path = NULL;
    }
    
    free(source);
    free(path);
}

// ======================================================
// [SECTION] EXECUTION
// ======================================================
static int findVar(const char* name) {
    for (int i = 0; i < vm.count; i++) {
        if (strcmp(vm.vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void evalAndPrint(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_INT:
            printf("%lld", node->data.int_val);
            break;
            
        case NODE_FLOAT:
            printf("%.2f", node->data.float_val);
            break;
            
        case NODE_STRING:
            printf("%s", node->data.str_val);
            break;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vm.vars[idx].is_float) {
                    printf("%.2f", vm.vars[idx].value.float_val);
                } else {
                    printf("%d", vm.vars[idx].value.int_val);
                }
            } else {
                printf(RED "[ERROR]" RESET " Undefined variable: %s\n", node->data.name);
            }
            break;
        }
            
        case NODE_BINARY: {
            // Pour l'affichage simple, on traite seulement les opérations de base
            if (node->left) evalAndPrint(node->left);
            printf(" ");
            switch ((TokenKind)node->data.int_val) {
                case TK_PLUS: printf("+"); break;
                case TK_MINUS: printf("-"); break;
                case TK_MULT: printf("*"); break;
                case TK_DIV: printf("/"); break;
                case TK_EQ: printf("=="); break;
                case TK_NEQ: printf("!="); break;
                default: printf("?"); break;
            }
            printf(" ");
            if (node->right) evalAndPrint(node->right);
            break;
        }
            
        default:
            break;
    }
}

static float evalFloat(ASTNode* node) {
    if (!node) return 0.0f;
    
    switch (node->type) {
        case NODE_INT:
            return (float)node->data.int_val;
            
        case NODE_FLOAT:
            return (float)node->data.float_val;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vm.vars[idx].is_float) {
                    return vm.vars[idx].value.float_val;
                } else {
                    return (float)vm.vars[idx].value.int_val;
                }
            }
            return 0.0f;
        }
            
        case NODE_BINARY: {
            float left = evalFloat(node->left);
            float right = evalFloat(node->right);
            
            switch ((TokenKind)node->data.int_val) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: return right != 0 ? left / right : 0;
                default: return 0;
            }
        }
            
        default:
            return 0.0f;
    }
}

static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_VAR: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                printf(RED "[ERROR]" RESET " Variable '%s' already exists\n", node->data.name);
                return;
            }
            
            if (vm.count < 100) {
                strncpy(vm.vars[vm.count].name, node->data.name, 49);
                vm.vars[vm.count].name[49] = '\0';
                
                if (node->left) {
                    if (node->left->type == NODE_FLOAT) {
                        vm.vars[vm.count].is_float = true;
                        vm.vars[vm.count].value.float_val = evalFloat(node->left);
                    } else {
                        vm.vars[vm.count].is_float = false;
                        vm.vars[vm.count].value.int_val = (int)evalFloat(node->left);
                    }
                } else {
                    vm.vars[vm.count].is_float = false;
                    vm.vars[vm.count].value.int_val = 0;
                }
                vm.count++;
            }
            break;
        }
            
        case NODE_ASSIGN: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (node->left) {
                    if (vm.vars[idx].is_float || 
                        (node->left->type == NODE_FLOAT)) {
                        vm.vars[idx].is_float = true;
                        vm.vars[idx].value.float_val = evalFloat(node->left);
                    } else {
                        vm.vars[idx].is_float = false;
                        vm.vars[idx].value.int_val = (int)evalFloat(node->left);
                    }
                }
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", node->data.name);
            }
            break;
        }
            
        case NODE_PRINT: {
            if (node->left) {
                evalAndPrint(node->left);
                printf("\n");
            }
            break;
        }
            
        case NODE_IMPORT: {
            for (int i = 0; i < node->import_count; i++) {
                if (node->data.imports[i]) {
                    importModule(node->data.imports[i], node->from_module);
                }
            }
            break;
        }
            
        case NODE_BLOCK:
            if (node->left) execute(node->left);
            if (node->right) execute(node->right);
            break;
            
        default:
            break;
    }
}

static void run(const char* source, const char* filename) {
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (nodes) {
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                execute(nodes[i]);
            }
        }
        
        // Cleanup
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                if (nodes[i]->type == NODE_IMPORT) {
                    for (int j = 0; j < nodes[i]->import_count; j++) {
                        free(nodes[i]->data.imports[j]);
                    }
                    free(nodes[i]->data.imports);
                    if (nodes[i]->from_module) free(nodes[i]->from_module);
                }
                if (nodes[i]->type == NODE_IDENT || 
                    nodes[i]->type == NODE_VAR || 
                    nodes[i]->type == NODE_ASSIGN) {
                    free(nodes[i]->data.name);
                }
                if (nodes[i]->type == NODE_STRING) {
                    free(nodes[i]->data.str_val);
                }
                free(nodes[i]);
            }
        }
        free(nodes);
    }
}

// ======================================================
// [SECTION] MAIN
// ======================================================
static void repl() {
    printf(GREEN "SwiftFlow" RESET " v1.0\n");
    printf("Type 'exit' to quit\n\n");
    
    setImportPath("./");
    
    char line[256];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, "exit") == 0) break;
        
        run(line, "REPL");
    }
    
    if (vm.import_path) free(vm.import_path);
}

int main(int argc, char* argv[]) {
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
        
        printf(GREEN "[SUCCES]" RESET ">> %s\n\n", argv[1]);
        
        // Définir le chemin
        char* file_path = my_strdup(argv[1]);
        char* last_slash = strrchr(file_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            setImportPath(file_path);
        } else {
            setImportPath("./");
        }
        free(file_path);
        
        run(source, argv[1]);
        free(source);
        
        if (vm.import_path) free(vm.import_path);
    }
    
    return 0;
}

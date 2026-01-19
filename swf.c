#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// ======================================================
// [SECTION] VM STATE
// ======================================================
typedef struct {
    char name[100];
    TokenKind type;
    int size_bytes;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
    } value;
    bool is_float;
    bool is_initialized;
    bool is_constant;
    int scope_level;
} Variable;

typedef struct {
    Variable vars[1000];
    int count;
    char* import_path;
    char* current_package;
    char* current_module;
    int scope_level;
} VM;

static VM vm = {0};

// ======================================================
// [SECTION] SYMBOL TABLE
// ======================================================
static Scope* current_scope = NULL;

static Scope* createScope(int level) {
    Scope* scope = malloc(sizeof(Scope));
    if (scope) {
        scope->level = level;
        scope->symbols = NULL;
        scope->parent = current_scope;
    }
    return scope;
}

static void enterScope() {
    vm.scope_level++;
    Scope* new_scope = createScope(vm.scope_level);
    if (new_scope) {
        new_scope->parent = current_scope;
        current_scope = new_scope;
    }
}

static void exitScope() {
    if (current_scope) {
        Scope* old = current_scope;
        current_scope = current_scope->parent;
        
        // Free symbols in old scope
        Symbol* sym = old->symbols;
        while (sym) {
            Symbol* next = sym->next;
            free(sym->name);
            free(sym->data_type);
            free(sym);
            sym = next;
        }
        
        free(old);
        vm.scope_level--;
    }
}

// ======================================================
// [SECTION] DBVAR TABLE
// ======================================================
static DBVarTable dbvar_table = {0};

static void initDBVarTable() {
    dbvar_table.capacity = 100;
    dbvar_table.entries = malloc(dbvar_table.capacity * sizeof(DBVarEntry));
    dbvar_table.count = 0;
}

static void addDBVarEntry(const char* name, const char* type, int size_bytes, 
                          const char* value_str, bool is_initialized) {
    if (dbvar_table.count >= dbvar_table.capacity) {
        dbvar_table.capacity *= 2;
        dbvar_table.entries = realloc(dbvar_table.entries, 
                                     dbvar_table.capacity * sizeof(DBVarEntry));
    }
    
    DBVarEntry* entry = &dbvar_table.entries[dbvar_table.count++];
    entry->name = str_copy(name);
    entry->type = str_copy(type);
    entry->size_bytes = size_bytes;
    entry->value_str = str_copy(value_str);
    entry->is_initialized = is_initialized;
    entry->line = 0;
    entry->scope = vm.scope_level;
}

static void displayDBVar() {
    printf("\n" CYAN "╔══════════════════════════════════════════════════════════════╗\n");
    printf("║                    TABLE DES VARIABLES (dbvar)                 ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    printf("║  Type     │ Nom         │ Taille     │ Valeur       │ Init   ║\n");
    printf("╠══════════════════════════════════════════════════════════════╣\n");
    
    for (int i = 0; i < dbvar_table.count; i++) {
        DBVarEntry* entry = &dbvar_table.entries[i];
        
        printf("║ %-9s │ %-11s │ %-10d │ %-12s │ %-6s ║\n",
               entry->type,
               entry->name,
               entry->size_bytes,
               entry->value_str,
               entry->is_initialized ? "✓" : "✗");
    }
    
    printf("╚══════════════════════════════════════════════════════════════╝\n" RESET);
}

static void freeDBVarTable() {
    for (int i = 0; i < dbvar_table.count; i++) {
        free(dbvar_table.entries[i].name);
        free(dbvar_table.entries[i].type);
        free(dbvar_table.entries[i].value_str);
    }
    free(dbvar_table.entries);
    dbvar_table.count = 0;
    dbvar_table.capacity = 0;
}

// ======================================================
// [SECTION] VARIABLE SIZE MANAGEMENT
// ======================================================
static int calculateVariableSize(TokenKind type) {
    switch (type) {
        case TK_VAR:
            return (rand() % 5) + 1; // 1-5 bytes
        
        case TK_NET:
            return (rand() % 8) + 1; // 1-8 bytes
        
        case TK_CLOG:
            return (rand() % 25) + 1; // 1-25 bytes
        
        case TK_DOS:
            return (rand() % 1024) + 1; // 1-1024 bytes
        
        case TK_SEL:
            return (rand() % 128) + 1; // 1-128 bytes
        
        case TK_TYPE_INT:
            return sizeof(int64_t);
        
        case TK_TYPE_FLOAT:
            return sizeof(double);
        
        case TK_TYPE_BOOL:
            return sizeof(bool);
        
        case TK_TYPE_STR:
            return 64; // Default string size
        
        default:
            return 4; // Default size
    }
}

static const char* getTypeName(TokenKind type) {
    switch (type) {
        case TK_VAR: return "var";
        case TK_NET: return "net";
        case TK_CLOG: return "clog";
        case TK_DOS: return "dos";
        case TK_SEL: return "sel";
        case TK_TYPE_INT: return "int";
        case TK_TYPE_FLOAT: return "float";
        case TK_TYPE_STR: return "string";
        case TK_TYPE_BOOL: return "bool";
        default: return "unknown";
    }
}

// ======================================================
// [SECTION] GLOBAL VARIABLES
// ======================================================
typedef struct {
    char name[100];
    union {
        int64_t int_val;
        double float_val;
    } value;
    bool is_float;
    char* module;
} GlobalVariable;

static GlobalVariable global_vars[500];
static int global_var_count = 0;

static int findGlobalVar(const char* name) {
    for (int i = 0; i < global_var_count; i++) {
        if (strcmp(global_vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void setGlobalVar(const char* name, double value, bool is_float, const char* module) {
    int idx = findGlobalVar(name);
    if (idx >= 0) {
        global_vars[idx].value.float_val = value;
        global_vars[idx].is_float = is_float;
    } else {
        if (global_var_count < 500) {
            strncpy(global_vars[global_var_count].name, name, 99);
            global_vars[global_var_count].name[99] = '\0';
            global_vars[global_var_count].value.float_val = value;
            global_vars[global_var_count].is_float = is_float;
            global_vars[global_var_count].module = module ? str_copy(module) : NULL;
            global_var_count++;
        }
    }
}

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

static void setCurrentPackage(const char* package) {
    if (vm.current_package) free(vm.current_package);
    vm.current_package = my_strdup(package);
}

static int findVar(const char* name) {
    // Search from current scope up
    for (int i = vm.count - 1; i >= 0; i--) {
        if (strcmp(vm.vars[i].name, name) == 0 && 
            vm.vars[i].scope_level <= vm.scope_level) {
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
                if (vm.vars[idx].is_float) {
                    return vm.vars[idx].value.float_val;
                } else {
                    return (double)vm.vars[idx].value.int_val;
                }
            }
            
            // Check global vars
            idx = findGlobalVar(node->data.name);
            if (idx >= 0) {
                if (global_vars[idx].is_float) {
                    return global_vars[idx].value.float_val;
                } else {
                    return (double)((int)global_vars[idx].value.float_val);
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
                case TK_POW: return pow(left, right);
                default: return 0.0;
            }
        }
            
        case NODE_UNARY: {
            double operand = evalFloat(node->left);
            switch (node->op_type) {
                case TK_MINUS: return -operand;
                case TK_NOT: return !operand;
                default: return operand;
            }
        }
            
        default:
            return 0.0;
    }
}

static char* evalString(ASTNode* node) {
    if (!node) return str_copy("");
    
    switch (node->type) {
        case NODE_STRING:
            return str_copy(node->data.str_val);
            
        case NODE_INT: {
            char* str = malloc(32);
            sprintf(str, "%lld", node->data.int_val);
            return str;
        }
            
        case NODE_FLOAT: {
            char* str = malloc(32);
            sprintf(str, "%.2f", node->data.float_val);
            return str;
        }
            
        case NODE_BOOL:
            return str_copy(node->data.bool_val ? "true" : "false");
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vm.vars[idx].is_float) {
                    char* str = malloc(32);
                    sprintf(str, "%.2f", vm.vars[idx].value.float_val);
                    return str;
                } else {
                    char* str = malloc(32);
                    sprintf(str, "%lld", vm.vars[idx].value.int_val);
                    return str;
                }
            }
            return str_copy("undefined");
        }
            
        default:
            return str_copy("");
    }
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
            
            int idx = findVar(node->data.name);
            if (idx >= 0 && vm.vars[idx].scope_level == vm.scope_level) {
                printf(RED "[ERROR]" RESET " Variable '%s' already declared in this scope\n", 
                       node->data.name);
                return;
            }
            
            if (vm.count < 1000) {
                Variable* var = &vm.vars[vm.count];
                strncpy(var->name, node->data.name, 99);
                var->name[99] = '\0';
                var->type = var_type;
                var->size_bytes = calculateVariableSize(var_type);
                var->scope_level = vm.scope_level;
                var->is_constant = (var_type == TK_CONST);
                
                if (node->left) {
                    var->is_initialized = true;
                    double val = evalFloat(node->left);
                    
                    if (node->left->type == NODE_FLOAT || 
                        node->left->type == NODE_STRING) {
                        var->is_float = true;
                        var->value.float_val = val;
                    } else {
                        var->is_float = false;
                        var->value.int_val = (int64_t)val;
                    }
                    
                    // Add to DBVar table
                    char value_str[32];
                    if (var->is_float) {
                        sprintf(value_str, "%.2f", var->value.float_val);
                    } else {
                        sprintf(value_str, "%lld", var->value.int_val);
                    }
                    
                    addDBVarEntry(var->name, getTypeName(var_type), 
                                 var->size_bytes, value_str, true);
                    
                    // Set global variable
                    setGlobalVar(node->data.name, val, var->is_float, 
                                vm.current_module);
                } else {
                    var->is_initialized = false;
                    var->is_float = false;
                    var->value.int_val = 0;
                    
                    addDBVarEntry(var->name, getTypeName(var_type), 
                                 var->size_bytes, "uninitialized", false);
                }
                
                vm.count++;
                printf(GREEN "[DECL]" RESET " %s %s declared (%d bytes)\n", 
                       getTypeName(var_type), var->name, var->size_bytes);
            }
            break;
        }
            
        case NODE_ASSIGN: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vm.vars[idx].is_constant) {
                    printf(RED "[ERROR]" RESET " Cannot assign to constant '%s'\n", 
                           node->data.name);
                    return;
                }
                
                if (node->left) {
                    vm.vars[idx].is_initialized = true;
                    double val = evalFloat(node->left);
                    
                    if (node->left->type == NODE_FLOAT) {
                        vm.vars[idx].is_float = true;
                        vm.vars[idx].value.float_val = val;
                    } else {
                        vm.vars[idx].is_float = false;
                        vm.vars[idx].value.int_val = (int64_t)val;
                    }
                    
                    // Update DBVar table
                    for (int i = 0; i < dbvar_table.count; i++) {
                        if (strcmp(dbvar_table.entries[i].name, node->data.name) == 0) {
                            free(dbvar_table.entries[i].value_str);
                            char value_str[32];
                            if (vm.vars[idx].is_float) {
                                sprintf(value_str, "%.2f", vm.vars[idx].value.float_val);
                            } else {
                                sprintf(value_str, "%lld", vm.vars[idx].value.int_val);
                            }
                            dbvar_table.entries[i].value_str = str_copy(value_str);
                            dbvar_table.entries[i].is_initialized = true;
                            break;
                        }
                    }
                    
                    // Update global variable
                    setGlobalVar(node->data.name, val, vm.vars[idx].is_float, 
                                vm.current_module);
                    
                    printf(CYAN "[ASSIGN]" RESET " %s = ", node->data.name);
                    if (vm.vars[idx].is_float) {
                        printf("%.2f\n", vm.vars[idx].value.float_val);
                    } else {
                        printf("%lld\n", vm.vars[idx].value.int_val);
                    }
                }
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", node->data.name);
            }
            break;
        }
            
        case NODE_PRINT: {
            if (node->left) {
                char* str = evalString(node->left);
                printf("%s", str);
                free(str);
            }
            printf("\n");
            break;
        }
            
        case NODE_SIZEOF: {
            int idx = findVar(node->data.size_info.var_name);
            if (idx >= 0) {
                printf("%d bytes", vm.vars[idx].size_bytes);
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", 
                       node->data.size_info.var_name);
            }
            break;
        }
            
        case NODE_DBVAR:
            displayDBVar();
            break;
            
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
            
        case NODE_FOR: {
            // Initializer
            if (node->left) {
                execute(node->left);
            }
            
            // Condition (stored in extra)
            while (node->extra && evalFloat(node->extra) != 0) {
                // Body
                execute(node->right);
                
                // Increment (stored in third)
                if (node->third) {
                    execute(node->third);
                }
            }
            break;
        }
            
        case NODE_BLOCK:
            enterScope();
            if (node->left) execute(node->left);
            if (node->right) execute(node->right);
            exitScope();
            break;
            
        case NODE_RETURN:
            // TODO: Implement return
            break;
            
        default:
            break;
    }
}

// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void run(const char* source, const char* filename) {
    // Initialize DBVar table
    initDBVarTable();
    
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (nodes) {
        // Find and execute main function first
        ASTNode* main_node = NULL;
        for (int i = 0; i < count; i++) {
            if (nodes[i] && nodes[i]->type == NODE_MAIN) {
                main_node = nodes[i];
                break;
            }
        }
        
        if (main_node) {
            printf(CYAN "[EXEC]" RESET " Starting main()...\n");
            execute(main_node);
        } else {
            // Execute all nodes in order
            for (int i = 0; i < count; i++) {
                if (nodes[i]) {
                    execute(nodes[i]);
                }
            }
        }
        
        // Cleanup
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                // Free AST node
                // TODO: Implement proper AST node cleanup
                free(nodes[i]);
            }
        }
        free(nodes);
    }
    
    // Free DBVar table
    freeDBVarTable();
}

// ======================================================
// [SECTION] REPL
// ======================================================
static void repl() {
    printf("                         SwiftFlow v2.0 - REPL Mode                  ║\n");

    printf("Commands: exit, dbvar, clear\n\n");
    
    char line[1024];
    while (1) {
        printf("swiftflow> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0) break;
        if (strcmp(line, "clear") == 0) {
            system("clear");
            continue;
        }
        if (strcmp(line, "dbvar") == 0) {
            displayDBVar();
            continue;
        }
        
        run(line, "REPL");
    }
    
    // Cleanup
    if (vm.import_path) free(vm.import_path);
    if (vm.current_package) free(vm.current_package);
    if (vm.current_module) free(vm.current_module);
}

// ======================================================
// [SECTION] MAIN
// ======================================================
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
        
        printf(GREEN "[SUCCESS]" RESET ">> Executing %s\n\n", argv[1]);
        
        run(source, argv[1]);
        free(source);
        
        // Cleanup
        if (vm.import_path) free(vm.import_path);
        if (vm.current_package) free(vm.current_package);
        if (vm.current_module) free(vm.current_module);
    }
    
    return 0;
}

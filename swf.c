#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// ======================================================
// [SECTION] VARIABLE SYSTEM WITH SCOPE
// ======================================================
typedef enum {
    VAR_VAR,
    VAR_NIP,
    VAR_SIM,
    VAR_NUUM
} VarType;

typedef struct Variable {
    char name[100];
    VarType type;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        char char_val;
        bool bool_val;
        void* json_val;
    } value;
    char type_name[50];
    bool is_set;
} Variable;

typedef struct Scope {
    Variable* vars;
    int capacity;
    int count;
    struct Scope* parent;
} Scope;

typedef struct {
    Scope* global_scope;
    Scope* current_scope;
    char* import_path;
    char* current_package;
    
    Scope** stack;
    int stack_size;
    int stack_capacity;
} VM;

static VM vm = {0};

// ======================================================
// [SECTION] SCOPE MANAGEMENT
// ======================================================
static Scope* scope_new(Scope* parent) {
    Scope* scope = calloc(1, sizeof(Scope));
    if (scope) {
        scope->capacity = 20;
        scope->vars = calloc(scope->capacity, sizeof(Variable));
        scope->parent = parent;
    }
    return scope;
}

static void scope_free(Scope* scope) {
    if (!scope) return;
    
    for (int i = 0; i < scope->count; i++) {
        if (strcmp(scope->vars[i].type_name, "string") == 0 && scope->vars[i].value.str_val) {
            free(scope->vars[i].value.str_val);
        }
    }
    
    free(scope->vars);
    free(scope);
}

static void vm_init() {
    if (!vm.global_scope) {
        vm.global_scope = scope_new(NULL);
        vm.current_scope = vm.global_scope;
        
        vm.stack_capacity = 10;
        vm.stack = malloc(vm.stack_capacity * sizeof(Scope*));
        vm.stack_size = 0;
        
        vm.stack[vm.stack_size++] = vm.global_scope;
    }
}

static void vm_push_scope() {
    Scope* new_scope = scope_new(vm.current_scope);
    vm.current_scope = new_scope;
    
    if (vm.stack_size >= vm.stack_capacity) {
        vm.stack_capacity *= 2;
        vm.stack = realloc(vm.stack, vm.stack_capacity * sizeof(Scope*));
    }
    vm.stack[vm.stack_size++] = new_scope;
}

static void vm_pop_scope() {
    if (vm.stack_size > 1) {
        Scope* old = vm.current_scope;
        vm.stack_size--;
        vm.current_scope = vm.stack[vm.stack_size - 1];
        scope_free(old);
    }
}

static Variable* find_variable(const char* name) {
    Scope* scope = vm.current_scope;
    
    while (scope) {
        for (int i = 0; i < scope->count; i++) {
            if (strcmp(scope->vars[i].name, name) == 0) {
                return &scope->vars[i];
            }
        }
        scope = scope->parent;
    }
    
    return NULL;
}

static Variable* create_variable(const char* name, VarType type) {
    Scope* scope = vm.current_scope;
    
    for (int i = 0; i < scope->count; i++) {
        if (strcmp(scope->vars[i].name, name) == 0) {
            printf(RED "[ERROR]" RESET " Variable '%s' already exists\n", name);
            return NULL;
        }
    }
    
    if (scope->count >= scope->capacity) {
        scope->capacity *= 2;
        scope->vars = realloc(scope->vars, scope->capacity * sizeof(Variable));
    }
    
    Variable* var = &scope->vars[scope->count++];
    strncpy(var->name, name, 99);
    var->name[99] = '\0';
    var->type = type;
    var->is_set = false;
    var->type_name[0] = '\0';
    
    return var;
}

// ======================================================
// [SECTION] PACKAGE SYSTEM
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

typedef struct {
    char alias[50];
    char module_path[100];
} PackageExport;

typedef struct {
    char name[50];
    PackageExport exports[20];
    int export_count;
} Package;

static Package packages[20];
static int package_count = 0;

static void register_package_export(const char* package_name, const char* alias, const char* module_path) {
    int pkg_idx = -1;
    for (int i = 0; i < package_count; i++) {
        if (strcmp(packages[i].name, package_name) == 0) {
            pkg_idx = i;
            break;
        }
    }
    
    if (pkg_idx == -1) {
        if (package_count < 20) {
            pkg_idx = package_count++;
            strncpy(packages[pkg_idx].name, package_name, 49);
            packages[pkg_idx].name[49] = '\0';
            packages[pkg_idx].export_count = 0;
        } else {
            printf(RED "[ERROR]" RESET " Too many packages\n");
            return;
        }
    }
    
    if (packages[pkg_idx].export_count < 20) {
        PackageExport* exp = &packages[pkg_idx].exports[packages[pkg_idx].export_count++];
        strncpy(exp->alias, alias, 49);
        exp->alias[49] = '\0';
        strncpy(exp->module_path, module_path, 99);
        exp->module_path[99] = '\0';
    }
}

// ======================================================
// [SECTION] IMPORT SYSTEM
// ======================================================
static char* my_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* d = malloc(len + 1);
    if (d) strcpy(d, s);
    return d;
}

static void set_import_path(const char* path) {
    if (vm.import_path) free(vm.import_path);
    vm.import_path = my_strdup(path);
}

static void set_current_package(const char* package) {
    if (vm.current_package) free(vm.current_package);
    vm.current_package = my_strdup(package);
}

static char* find_import_file(const char* module, const char* from_package) {
    if (module[0] == '.' || module[0] == '/') {
        if (module[0] == '.' && vm.current_package) {
            char* full_path = malloc(strlen("/usr/local/lib/swift/") + 
                                    strlen(vm.current_package) + 
                                    strlen(module) + 10);
            if (!full_path) return NULL;
            
            const char* actual_module = module;
            if (strncmp(module, "./", 2) == 0) {
                actual_module = module + 2;
            }
            
            sprintf(full_path, "/usr/local/lib/swift/%s/%s", 
                    vm.current_package, actual_module);
            return full_path;
        }
        return my_strdup(module);
    }
    
    const char* base_path = "/usr/local/lib/swift/";
    char* path = malloc(strlen(base_path) + strlen(module) + 10);
    
    sprintf(path, "%s%s.swf", base_path, module);
    if (access(path, F_OK) == 0) return path;
    
    sprintf(path, "%s%s", base_path, module);
    if (access(path, F_OK) == 0) return path;
    
    if (vm.current_package) {
        sprintf(path, "/usr/local/lib/swift/%s/%s", vm.current_package, module);
        if (access(path, F_OK) == 0) return path;
        
        sprintf(path, "/usr/local/lib/swift/%s/%s.swf", vm.current_package, module);
        if (access(path, F_OK) == 0) return path;
    }
    
    free(path);
    return NULL;
}

static void import_module(const char* module, const char* from_package);
static void run(const char* source, const char* filename);

static void import_module(const char* module, const char* from_package) {
    printf(YELLOW "[IMPORT]" RESET " %s", module);
    if (from_package) printf(" from %s", from_package);
    printf("\n");
    
    char* path = find_import_file(module, from_package);
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
    
    const char* ext = strrchr(path, '.');
    bool is_package = ext && strcmp(ext, ".svlib") == 0;
    
    char* old_path = vm.import_path ? my_strdup(vm.import_path) : NULL;
    char* old_package = vm.current_package ? my_strdup(vm.current_package) : NULL;
    
    char* dir_path = my_strdup(path);
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        if (is_package) {
            *last_slash = '\0';
            char* package_name = strrchr(dir_path, '/');
            if (package_name) {
                set_current_package(package_name + 1);
            } else {
                set_current_package(dir_path);
            }
        } else {
            *last_slash = '\0';
        }
        set_import_path(dir_path);
    }
    free(dir_path);
    
    run(source, module);
    
    if (old_path) {
        set_import_path(old_path);
        free(old_path);
    } else if (vm.import_path) {
        free(vm.import_path);
        vm.import_path = NULL;
    }
    
    if (old_package) {
        set_current_package(old_package);
        free(old_package);
    } else if (vm.current_package) {
        free(vm.current_package);
        vm.current_package = NULL;
    }
    
    free(source);
    free(path);
}

// ======================================================
// [SECTION] EXPRESSION EVALUATION
// ======================================================
static int64_t eval_int(ASTNode* node) {
    if (!node) return 0;
    
    switch (node->type) {
        case NODE_INT:
            return node->data.int_val;
        case NODE_FLOAT:
            return (int64_t)node->data.float_val;
        case NODE_CHAR:
            return (int64_t)node->data.char_val;
        case NODE_BOOL:
            return node->data.bool_val ? 1 : 0;
        case NODE_IDENT: {
            Variable* var = find_variable(node->data.name);
            if (var) {
                if (strcmp(var->type_name, "int") == 0) {
                    return var->value.int_val;
                } else if (strcmp(var->type_name, "float") == 0) {
                    return (int64_t)var->value.float_val;
                } else if (strcmp(var->type_name, "char") == 0) {
                    return (int64_t)var->value.char_val;
                } else if (strcmp(var->type_name, "bool") == 0) {
                    return var->value.bool_val ? 1 : 0;
                }
            }
            printf(RED "[ERROR]" RESET " Undefined variable: %s\n", node->data.name);
            return 0;
        }
        case NODE_BINARY: {
            int64_t left = eval_int(node->left);
            int64_t right = eval_int(node->right);
            
            switch ((TokenKind)node->data.int_val) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: return right != 0 ? left / right : 0;
                case TK_MOD: return right != 0 ? left % right : 0;
                default: return 0;
            }
        }
        default:
            return 0;
    }
}

static double eval_float(ASTNode* node) {
    if (!node) return 0.0;
    
    switch (node->type) {
        case NODE_INT:
            return (double)node->data.int_val;
        case NODE_FLOAT:
            return node->data.float_val;
        case NODE_IDENT: {
            Variable* var = find_variable(node->data.name);
            if (var) {
                if (strcmp(var->type_name, "float") == 0) {
                    return var->value.float_val;
                } else if (strcmp(var->type_name, "int") == 0) {
                    return (double)var->value.int_val;
                }
            }
            return 0.0;
        }
        case NODE_BINARY: {
            double left = eval_float(node->left);
            double right = eval_float(node->right);
            
            switch ((TokenKind)node->data.int_val) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: return right != 0.0 ? left / right : 0.0;
                default: return 0.0;
            }
        }
        default:
            return 0.0;
    }
}

static char* eval_string(ASTNode* node) {
    if (!node) return str_copy("");
    
    switch (node->type) {
        case NODE_STRING:
            return str_copy(node->data.str_val);
        case NODE_IDENT: {
            Variable* var = find_variable(node->data.name);
            if (var && strcmp(var->type_name, "string") == 0) {
                return str_copy(var->value.str_val);
            }
            return str_copy("");
        }
        case NODE_CHAR: {
            char* str = malloc(2);
            if (str) {
                str[0] = node->data.char_val;
                str[1] = '\0';
            }
            return str;
        }
        default: {
            char buffer[100];
            if (node->type == NODE_INT) {
                snprintf(buffer, sizeof(buffer), "%lld", eval_int(node));
            } else if (node->type == NODE_FLOAT) {
                snprintf(buffer, sizeof(buffer), "%f", eval_float(node));
            } else if (node->type == NODE_BOOL) {
                snprintf(buffer, sizeof(buffer), "%s", node->data.bool_val ? "true" : "false");
            } else {
                snprintf(buffer, sizeof(buffer), "<?>");
            }
            return str_copy(buffer);
        }
    }
}

// ======================================================
// [SECTION] STATEMENT EXECUTION
// ======================================================
static void execute(ASTNode* node);

static void execute_var_decl(ASTNode* node, VarType var_type) {
    if (!node || !node->data.name) return;
    
    Variable* var = create_variable(node->data.name, var_type);
    if (!var) return;
    
    if (node->left) {
        switch (node->left->type) {
            case NODE_INT:
                strcpy(var->type_name, "int");
                var->value.int_val = eval_int(node->left);
                break;
            case NODE_FLOAT:
                strcpy(var->type_name, "float");
                var->value.float_val = eval_float(node->left);
                break;
            case NODE_STRING:
                strcpy(var->type_name, "string");
                var->value.str_val = eval_string(node->left);
                break;
            case NODE_CHAR:
                strcpy(var->type_name, "char");
                var->value.char_val = node->left->data.char_val;
                break;
            case NODE_BOOL:
                strcpy(var->type_name, "bool");
                var->value.bool_val = node->left->data.bool_val;
                break;
            default:
                strcpy(var->type_name, "unknown");
                break;
        }
        var->is_set = true;
    }
    
    if (vm.current_package && var_type == VAR_NIP && node->left && 
        node->left->type == NODE_STRING) {
        register_package_export(vm.current_package, node->data.name, 
                               node->left->data.str_val);
    }
}

static void execute_print(ASTNode* node) {
    if (!node) return;
    
    if (node->left) {
        char* str = eval_string(node->left);
        if (str) {
            printf("%s", str);
            free(str);
        }
    }
    
    if (node->right && node->right->type == NODE_PRINT) {
        printf(" ");
        execute_print(node->right);
    } else if (node->right) {
        execute_print(node->right);
    }
}

static void execute_assignment(ASTNode* node) {
    if (!node || !node->data.name) return;
    
    Variable* var = find_variable(node->data.name);
    if (!var) {
        printf(RED "[ERROR]" RESET " Variable '%s' not found\n", node->data.name);
        return;
    }
    
    if (var->type == VAR_NIP && var->is_set) {
        printf(RED "[ERROR]" RESET " Variable '%s' is immutable (nip)\n", node->data.name);
        return;
    }
    
    if (var->type == VAR_NUUM && node->left) {
        if (node->left->type != NODE_INT && node->left->type != NODE_FLOAT) {
            printf(RED "[ERROR]" RESET " Variable '%s' accepts only numbers (nuum)\n", node->data.name);
            return;
        }
    }
    
    if (node->left) {
        if (strcmp(var->type_name, "int") == 0) {
            var->value.int_val = eval_int(node->left);
        } else if (strcmp(var->type_name, "float") == 0) {
            var->value.float_val = eval_float(node->left);
        } else if (strcmp(var->type_name, "string") == 0) {
            if (var->value.str_val) free(var->value.str_val);
            var->value.str_val = eval_string(node->left);
        } else if (strcmp(var->type_name, "char") == 0) {
            var->value.char_val = (char)eval_int(node->left);
        } else if (strcmp(var->type_name, "bool") == 0) {
            var->value.bool_val = eval_int(node->left) != 0;
        }
        var->is_set = true;
    }
}

static void execute_sizeof(ASTNode* node) {
    if (!node) return;
    
    if (node->data.size_op.expr && node->data.size_op.expr->type == NODE_IDENT) {
        Variable* var = find_variable(node->data.size_op.expr->data.name);
        if (var) {
            int size = 0;
            if (strcmp(var->type_name, "int") == 0) size = sizeof(int64_t);
            else if (strcmp(var->type_name, "float") == 0) size = sizeof(double);
            else if (strcmp(var->type_name, "char") == 0) size = sizeof(char);
            else if (strcmp(var->type_name, "bool") == 0) size = sizeof(bool);
            else if (strcmp(var->type_name, "string") == 0 && var->value.str_val) 
                size = strlen(var->value.str_val);
            
            printf("%d", size);
        }
    }
}

static void execute_zis(ASTNode* node) {
    if (!node) return;
    
    if (node->data.size_op.expr && node->data.size_op.expr->type == NODE_IDENT) {
        Variable* var = find_variable(node->data.size_op.expr->data.name);
        if (var) {
            int size = 0;
            if (strcmp(var->type_name, "int") == 0) size = sizeof(int64_t);
            else if (strcmp(var->type_name, "float") == 0) size = sizeof(double);
            else if (strcmp(var->type_name, "char") == 0) size = sizeof(char);
            else if (strcmp(var->type_name, "bool") == 0) size = sizeof(bool);
            else if (strcmp(var->type_name, "string") == 0) 
                size = sizeof(char*) + (var->value.str_val ? strlen(var->value.str_val) + 1 : 0);
            
            printf("%d", size);
        }
    }
}

static void execute_if(ASTNode* node) {
    if (!node) return;
    
    if (node->left) {
        int64_t cond = eval_int(node->left);
        if (cond != 0) {
            if (node->right) execute(node->right);
        } else if (node->right && node->right->type == NODE_BLOCK) {
            if (node->right->right) execute(node->right->right);
        }
    }
}

static void execute_while(ASTNode* node) {
    if (!node) return;
    
    while (node->left && eval_int(node->left) != 0) {
        if (node->right) execute(node->right);
    }
}

static void execute_for(ASTNode* node) {
    if (!node) return;
    
    if (node->left) execute(node->left);
    
    while (node->data.func.body && node->data.func.body->left && 
           eval_int(node->data.func.body->left) != 0) {
        if (node->right) execute(node->right);
        
        if (node->data.func.body && node->data.func.body->right) {
            execute(node->data.func.body->right);
        }
    }
}

static void execute_block(ASTNode* node) {
    if (!node) return;
    
    vm_push_scope();
    
    if (node->left) execute(node->left);
    if (node->right) execute(node->right);
    
    vm_pop_scope();
}

static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_VAR:
            execute_var_decl(node, VAR_VAR);
            break;
        case NODE_NIP:
            execute_var_decl(node, VAR_NIP);
            break;
        case NODE_SIM:
            execute_var_decl(node, VAR_SIM);
            break;
        case NODE_NUUM:
            execute_var_decl(node, VAR_NUUM);
            break;
        case NODE_ASSIGN:
            execute_assignment(node);
            break;
        case NODE_PRINT:
            execute_print(node);
            printf("\n");
            break;
        case NODE_IMPORT:
            for (int i = 0; i < node->import_count; i++) {
                if (node->data.imports[i]) {
                    import_module(node->data.imports[i], node->from_module);
                }
            }
            break;
        case NODE_IF:
            execute_if(node);
            break;
        case NODE_WHILE:
            execute_while(node);
            break;
        case NODE_FOR:
            execute_for(node);
            break;
        case NODE_BLOCK:
            execute_block(node);
            break;
        case NODE_MAIN:
            vm_push_scope();
            if (node->left) execute(node->left);
            vm_pop_scope();
            break;
        case NODE_SIZEOF:
            execute_sizeof(node);
            break;
        case NODE_ZIS:
            execute_zis(node);
            break;
        case NODE_CLASS:
            printf("[CLASS] Defined: %s\n", node->data.class.name);
            break;
        case NODE_FUNC:
            printf("[FUNC] Defined: %s\n", node->data.func.name);
            break;
        case NODE_RETURN:
            if (node->left) {
                printf("[RETURN] ");
                if (node->left->type == NODE_INT) printf("%lld\n", node->left->data.int_val);
                else if (node->left->type == NODE_FLOAT) printf("%f\n", node->left->data.float_val);
            }
            break;
        default:
            break;
    }
}

// ======================================================
// [SECTION] MAIN EXECUTION
// ======================================================
static void run(const char* source, const char* filename) {
    (void)filename;
    
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (nodes) {
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                execute(nodes[i]);
            }
        }
        
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
                    nodes[i]->type == NODE_NIP ||
                    nodes[i]->type == NODE_SIM ||
                    nodes[i]->type == NODE_NUUM ||
                    nodes[i]->type == NODE_ASSIGN ||
                    nodes[i]->type == NODE_FUNC ||
                    nodes[i]->type == NODE_CLASS) {
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
// [SECTION] REPL
// ======================================================
static void repl() {
    printf(GREEN "SwiftFlow" RESET " v2.0 Extended\n");
    printf("Variables: var, nip(immutable), sim(simple), nuum(number)\n");
    printf("Type 'exit' to quit\n\n");
    
    set_import_path("./");
    vm_init();
    
    char line[1024];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, "exit") == 0) break;
        
        run(line, "REPL");
    }
    
    if (vm.import_path) free(vm.import_path);
    if (vm.current_package) free(vm.current_package);
}

// ======================================================
// [SECTION] MAIN FUNCTION
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
        
        printf(GREEN "[SUCCESS]" RESET ">> %s\n\n", argv[1]);
        
        char* file_path = my_strdup(argv[1]);
        char* last_slash = strrchr(file_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            set_import_path(file_path);
        } else {
            set_import_path("./");
        }
        free(file_path);
        
        vm_init();
        run(source, argv[1]);
        free(source);
        
        if (vm.import_path) free(vm.import_path);
        if (vm.current_package) free(vm.current_package);
    }
    
    return 0;
}

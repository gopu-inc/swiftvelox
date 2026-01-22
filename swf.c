#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <unistd.h>
#include <libgen.h>
#include <limits.h>
#include <errno.h>
#include <ctype.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// ======================================================
// [SECTION] SYSTEM CONFIGURATION
// ======================================================
#define SWIFT_HOME "/usr/local/lib/swift"
#define MAX_FUNCTIONS 500
#define MAX_VARIABLES 2000
#define MAX_CLASSES 100
#define MAX_IMPORTS 200
#define MAX_SCOPES 100

// ======================================================
// [SECTION] VARIABLE SYSTEM
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
        void* ptr_val;
    } value;
    bool is_float;
    bool is_string;
    bool is_initialized;
    bool is_constant;
    int scope_level;
    char* module;
    bool is_exported;
} Variable;

static Variable vars[MAX_VARIABLES];
static int var_count = 0;
static int scope_level = 0;
static int scope_stack[MAX_SCOPES];
static int scope_stack_ptr = 0;

// ======================================================
// [SECTION] FUNCTION SYSTEM
// ======================================================
typedef struct {
    char name[200];  // Format: "module.func" ou "package.module.func"
    ASTNode* params;
    ASTNode* body;
    int param_count;
    char** param_names;
    char* return_type;
    double return_value;
    char* return_string;
    bool has_returned;
    char* module_name;
    bool is_exported;
} Function;

static Function functions[MAX_FUNCTIONS];
static int func_count = 0;
static Function* current_function = NULL;

// ======================================================
// [SECTION] CLASS SYSTEM
// ======================================================
typedef struct {
    char name[100];
    char* parent;
    ASTNode* members;
    char* module;
    bool is_exported;
} Class;

static Class classes[MAX_CLASSES];
static int class_count = 0;

// ======================================================
// [SECTION] IMPORT SYSTEM
// ======================================================
typedef struct {
    char* module_path;    // Chemin complet
    char* alias;          // Alias d'import
    char* from_package;   // Package source
    bool is_loaded;
    char** exported_funcs;
    int export_count;
} ImportedModule;

static ImportedModule imports[MAX_IMPORTS];
static int import_count = 0;
static char current_working_dir[PATH_MAX];

// ======================================================
// [SECTION] NATIVE FUNCTION REGISTRY
// ======================================================
typedef struct {
    char* name;
    void* (*func_ptr)(void**);
    int param_count;
    char** param_types;
    char* return_type;
} NativeFunction;

static NativeFunction native_functions[100];
static int native_func_count = 0;

// ======================================================
// [SECTION] FUNCTION DECLARATIONS
// ======================================================
static void execute(ASTNode* node);
static double evalFloat(ASTNode* node);
static char* evalString(ASTNode* node);
static bool evalBool(ASTNode* node);
static void* evalAny(ASTNode* node);
static void initWorkingDir(const char* filename);
static bool loadModule(const char* import_path, const char* from_package, const char* alias);
static bool loadPackage(const char* package_name);
static void registerNativeFunctions();
static void pushScope();
static void popScope();
static int findVar(const char* name);
static void registerFunction(const char* name, ASTNode* params, ASTNode* body, 
                             int param_count, const char* module, bool is_exported);
static Function* findFunction(const char* name);
static void registerClass(const char* name, char* parent, ASTNode* members, 
                         const char* module, bool is_exported);
static Class* findClass(const char* name);
static char* resolveModulePath(const char* import_path, const char* from_package);

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static void initWorkingDir(const char* filename) {
    if (filename && strcmp(filename, "REPL") != 0) {
        char abs_path[PATH_MAX];
        if (realpath(filename, abs_path)) {
            char* dir_copy = strdup(abs_path);
            if (dir_copy) {
                char* dir = dirname(dir_copy);
                strncpy(current_working_dir, dir, sizeof(current_working_dir) - 1);
                free(dir_copy);
            }
        }
    }
    if (current_working_dir[0] == '\0') {
        getcwd(current_working_dir, sizeof(current_working_dir));
    }
}

static void pushScope() {
    if (scope_stack_ptr < MAX_SCOPES) {
        scope_stack[scope_stack_ptr++] = var_count;
        scope_level++;
    }
}

static void popScope() {
    if (scope_stack_ptr > 0) {
        int prev_count = scope_stack[--scope_stack_ptr];
        // Remove variables from popped scope
        for (int i = var_count - 1; i >= prev_count; i--) {
            if (vars[i].is_string && vars[i].value.str_val) {
                free(vars[i].value.str_val);
            }
        }
        var_count = prev_count;
        scope_level--;
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

static const char* getTypeName(TokenKind type) {
    switch (type) {
        case TK_TYPE_INT: return "int";
        case TK_TYPE_FLOAT: return "float";
        case TK_TYPE_STR: return "string";
        case TK_TYPE_BOOL: return "bool";
        case TK_TYPE_CHAR: return "char";
        case TK_TYPE_VOID: return "void";
        default: return "unknown";
    }
}

// ======================================================
// [SECTION] FUNCTION REGISTRATION
// ======================================================
static void registerFunction(const char* name, ASTNode* params, ASTNode* body, 
                             int param_count, const char* module, bool is_exported) {
    if (func_count < MAX_FUNCTIONS) {
        Function* func = &functions[func_count];
        
        // Format: module.func ou package.module.func
        if (module && module[0] != '\0') {
            snprintf(func->name, sizeof(func->name), "%s.%s", module, name);
        } else {
            strncpy(func->name, name, sizeof(func->name) - 1);
        }
        
        func->params = params;
        func->body = body;
        func->param_count = param_count;
        func->module_name = module ? str_copy(module) : NULL;
        func->is_exported = is_exported;
        func->has_returned = false;
        func->return_value = 0;
        func->return_string = NULL;
        func->return_type = NULL;
        
        // Extract parameter names
        if (param_count > 0 && params) {
            func->param_names = malloc(param_count * sizeof(char*));
            ASTNode* param = params;
            for (int i = 0; i < param_count && param; i++) {
                if (param->type == NODE_IDENT && param->data.name) {
                    func->param_names[i] = str_copy(param->data.name);
                } else {
                    func->param_names[i] = NULL;
                }
                param = param->right;
            }
        } else {
            func->param_names = NULL;
        }
        
        func_count++;
    }
}

static Function* findFunction(const char* name) {
    // Try exact match first
    for (int i = 0; i < func_count; i++) {
        if (strcmp(functions[i].name, name) == 0) {
            return &functions[i];
        }
    }
    
    // Try without module prefix
    const char* dot = strrchr(name, '.');
    if (dot) {
        const char* func_name = dot + 1;
        for (int i = 0; i < func_count; i++) {
            const char* fdot = strrchr(functions[i].name, '.');
            if (fdot && strcmp(fdot + 1, func_name) == 0) {
                return &functions[i];
            }
        }
    }
    
    return NULL;
}

static void registerClass(const char* name, char* parent, ASTNode* members, 
                         const char* module, bool is_exported) {
    if (class_count < MAX_CLASSES) {
        Class* cls = &classes[class_count];
        strncpy(cls->name, name, sizeof(cls->name) - 1);
        cls->parent = parent ? str_copy(parent) : NULL;
        cls->members = members;
        cls->module = module ? str_copy(module) : NULL;
        cls->is_exported = is_exported;
        class_count++;
    }
}

// ======================================================
// [SECTION] MODULE RESOLUTION
// ======================================================
static char* resolveModulePath(const char* import_path, const char* from_package) {
    char* resolved = malloc(PATH_MAX);
    if (!resolved) return NULL;
    
    // Local imports (./, ../, /)
    if (import_path[0] == '.') {
        if (import_path[1] == '/') {
            snprintf(resolved, PATH_MAX, "%s/%s.swf", current_working_dir, import_path + 2);
        } else if (import_path[1] == '.' && import_path[2] == '/') {
            char parent_dir[PATH_MAX];
            strncpy(parent_dir, current_working_dir, sizeof(parent_dir));
            char* last_slash = strrchr(parent_dir, '/');
            if (last_slash) *last_slash = '\0';
            snprintf(resolved, PATH_MAX, "%s/%s.swf", parent_dir, import_path + 3);
        } else {
            snprintf(resolved, PATH_MAX, "%s/%s.swf", current_working_dir, import_path);
        }
    }
    // Absolute path
    else if (import_path[0] == '/') {
        snprintf(resolved, PATH_MAX, "%s.swf", import_path);
    }
    // Package import
    else if (from_package && from_package[0] != '\0') {
        snprintf(resolved, PATH_MAX, "%s/%s/%s.swf", SWIFT_HOME, from_package, import_path);
    }
    // Standard library or module
    else {
        // Try in stdlib first
        snprintf(resolved, PATH_MAX, "%s/stdlib/%s.swf", SWIFT_HOME, import_path);
        if (access(resolved, F_OK) != 0) {
            // Try in modules directory
            snprintf(resolved, PATH_MAX, "%s/modules/%s.swf", SWIFT_HOME, import_path);
        }
    }
    
    return resolved;
}

// ======================================================
// [SECTION] MODULE LOADING
// ======================================================
static bool loadModule(const char* import_path, const char* from_package, const char* alias) {
    char* module_path = resolveModulePath(import_path, from_package);
    if (!module_path) {
        printf("[ERROR] Failed to resolve import: %s\n", import_path);
        return false;
    }
    
    // Check if already loaded
    for (int i = 0; i < import_count; i++) {
        if (strcmp(imports[i].module_path, module_path) == 0) {
            free(module_path);
            return true; // Already loaded
        }
    }
    
    printf("[IMPORT] Loading: %s\n", module_path);
    
    FILE* f = fopen(module_path, "r");
    if (!f) {
        printf("[ERROR] Cannot open module: %s\n", module_path);
        free(module_path);
        return false;
    }
    
    // Read module source
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    // Save current directory
    char old_dir[PATH_MAX];
    strncpy(old_dir, current_working_dir, sizeof(old_dir));
    
    // Set module directory
    strncpy(current_working_dir, module_path, sizeof(current_working_dir));
    char* dir = dirname(current_working_dir);
    strncpy(current_working_dir, dir, sizeof(current_working_dir));
    
    // Parse module
    int node_count = 0;
    ASTNode** nodes = parse(source, &node_count);
    
    // Extract module name from path
    char module_name[100];
    char* base = basename(module_path);
    char* dot = strrchr(base, '.');
    if (dot) *dot = '\0';
    strncpy(module_name, base, sizeof(module_name) - 1);
    
    // Register exports from module
    if (nodes) {
        for (int i = 0; i < node_count; i++) {
            if (nodes[i]->type == NODE_FUNC) {
                // Check if function is exported
                bool is_exported = false;
                ASTNode* attrs = nodes[i]->fourth; // Assuming attributes stored in fourth
                while (attrs) {
                    if (attrs->type == NODE_IDENT && strcmp(attrs->data.name, "export") == 0) {
                        is_exported = true;
                        break;
                    }
                    attrs = attrs->right;
                }
                
                // Count parameters
                int param_count = 0;
                ASTNode* param = nodes[i]->left;
                while (param) {
                    param_count++;
                    param = param->right;
                }
                
                registerFunction(nodes[i]->data.name, nodes[i]->left, nodes[i]->right, 
                                param_count, module_name, is_exported);
            }
            else if (nodes[i]->type == NODE_CLASS) {
                // Similar handling for classes
                registerClass(nodes[i]->data.class_def.name, 
                            nodes[i]->data.class_def.parent ? 
                            nodes[i]->data.class_def.parent->data.name : NULL,
                            nodes[i]->data.class_def.members,
                            module_name, false); // TODO: Handle class exports
            }
            
            // Free the node
            free(nodes[i]);
        }
        free(nodes);
    }
    
    // Restore directory
    strncpy(current_working_dir, old_dir, sizeof(current_working_dir));
    
    // Register import
    if (import_count < MAX_IMPORTS) {
        ImportedModule* imp = &imports[import_count];
        imp->module_path = str_copy(module_path);
        imp->alias = alias ? str_copy(alias) : NULL;
        imp->from_package = from_package ? str_copy(from_package) : NULL;
        imp->is_loaded = true;
        imp->exported_funcs = NULL;
        imp->export_count = 0;
        import_count++;
    }
    
    free(source);
    free(module_path);
    return true;
}

// ======================================================
// [SECTION] NATIVE FUNCTION IMPLEMENTATIONS
// ======================================================
static void* native_print(void** args) {
    for (int i = 0; args[i] != NULL; i++) {
        char* str = args[i];
        printf("%s", str);
        if (args[i + 1] != NULL) printf(" ");
    }
    printf("\n");
    return NULL;
}

static void* native_readfile(void** args) {
    if (!args[0]) return NULL;
    char* filename = args[0];
    
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("[ERROR] Cannot read file: %s\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    
    return content;
}

static void* native_writefile(void** args) {
    if (!args[0] || !args[1]) return NULL;
    char* filename = args[0];
    char* content = args[1];
    bool append = args[2] ? *(bool*)args[2] : false;
    
    FILE* f = fopen(filename, append ? "a" : "w");
    if (!f) {
        printf("[ERROR] Cannot write file: %s\n", filename);
        return NULL;
    }
    
    fwrite(content, 1, strlen(content), f);
    fclose(f);
    
    int* result = malloc(sizeof(int));
    *result = strlen(content);
    return result;
}

static void registerNativeFunctions() {
    // Print function
    native_functions[native_func_count].name = "print";
    native_functions[native_func_count].func_ptr = native_print;
    native_functions[native_func_count].param_count = -1; // Variable args
    native_functions[native_func_count].return_type = "void";
    native_func_count++;
    
    // File functions
    native_functions[native_func_count].name = "__native_readfile";
    native_functions[native_func_count].func_ptr = native_readfile;
    native_functions[native_func_count].param_count = 1;
    native_functions[native_func_count].param_types = malloc(sizeof(char*));
    native_functions[native_func_count].param_types[0] = "string";
    native_functions[native_func_count].return_type = "string";
    native_func_count++;
    
    native_functions[native_func_count].name = "__native_writefile";
    native_functions[native_func_count].func_ptr = native_writefile;
    native_functions[native_func_count].param_count = 3;
    native_functions[native_func_count].param_types = malloc(3 * sizeof(char*));
    native_functions[native_func_count].param_types[0] = "string";
    native_functions[native_func_count].param_types[1] = "string";
    native_functions[native_func_count].param_types[2] = "bool";
    native_functions[native_func_count].return_type = "int";
    native_func_count++;
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
        case NODE_STRING: {
            char* endptr;
            return strtod(node->data.str_val, &endptr);
        }
        case NODE_IDENT: {
    printf("[CRITICAL DEBUG] Reading variable: %s\n", node->data.name);
    int idx = findVar(node->data.name);
    if (idx >= 0) {
        double val;
        if (vars[idx].is_float) {
            val = vars[idx].value.float_val;
            printf("[CRITICAL DEBUG] Found %s as float: %f\n", node->data.name, val);
        } else if (vars[idx].is_string) {
            val = 0.0; // Conversion simplifiée
            printf("[CRITICAL DEBUG] Found %s as string\n", node->data.name);
        } else {
            val = (double)vars[idx].value.int_val;
            printf("[CRITICAL DEBUG] Found %s as int: %lld -> %f\n", 
                   node->data.name, vars[idx].value.int_val, val);
        }
        return val;
    }
    printf("[CRITICAL DEBUG] Variable %s NOT FOUND\n", node->data.name);
    return 0.0;
}

        case NODE_BINARY: {
    printf("[DEBUG BINARY] Starting binary operation, operator type: %d\n", node->op_type);
    
    // Évaluer les côtés gauche et droit
    double left_val = evalFloat(node->left);
    double right_val = evalFloat(node->right);
    
    printf("[DEBUG BINARY] Left value: %f, Right value: %f\n", left_val, right_val);
    
    // Appliquer l'opérateur
    switch (node->op_type) {
        // Opérateurs arithmétiques
        case TK_PLUS: {
    double left_val = evalFloat(node->left);
    double right_val = evalFloat(node->right);
    double result = left_val + right_val;
    
    // Debug détaillé
    printf("[DEBUG BINARY PLUS] ");
    if (node->left->type == NODE_IDENT) {
        printf("%s", node->left->data.name);
    } else {
        printf("%f", left_val);
    }
    printf(" + ");
    if (node->right->type == NODE_IDENT) {
        printf("%s", node->right->data.name);
    } else {
        printf("%f", right_val);
    }
    printf(" = %f\n", result);
    
    return result;
}
        case TK_MINUS: {
            double result = left_val - right_val;
            printf("[DEBUG BINARY] %f - %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_MULT: {
            double result = left_val * right_val;
            printf("[DEBUG BINARY] %f * %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_DIV: {
            if (right_val == 0.0) {
                printf("[DEBUG BINARY] Division by zero: %f / 0\n", left_val);
                return INFINITY;
            }
            double result = left_val / right_val;
            printf("[DEBUG BINARY] %f / %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_MOD: {
            if (right_val == 0.0) {
                printf("[DEBUG BINARY] Modulo by zero: %f %% 0\n", left_val);
                return 0.0;
            }
            double result = fmod(left_val, right_val);
            printf("[DEBUG BINARY] %f %% %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_POW: {
            double result = pow(left_val, right_val);
            printf("[DEBUG BINARY] %f ^ %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_CONCAT: {
            // Concaténation de strings - convertit en string puis évalue
            char* left_str = evalString(node->left);
            char* right_str = evalString(node->right);
            char* combined = malloc(strlen(left_str) + strlen(right_str) + 1);
            strcpy(combined, left_str);
            strcat(combined, right_str);
            
            printf("[DEBUG BINARY] String concat: \"%s\" + \"%s\" = \"%s\"\n", 
                   left_str, right_str, combined);
            
            // Convertir en nombre si possible
            char* endptr;
            double result = strtod(combined, &endptr);
            
            free(left_str);
            free(right_str);
            free(combined);
            
            if (endptr != combined) {
                return result;
            }
            return 0.0;
        }
        
        // Opérateurs de comparaison
        case TK_EQ: {
            double result = (left_val == right_val) ? 1.0 : 0.0;
            printf("[DEBUG BINARY] %f == %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_NEQ: {
            double result = (left_val != right_val) ? 1.0 : 0.0;
            printf("[DEBUG BINARY] %f != %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_GT: {
            double result = (left_val > right_val) ? 1.0 : 0.0;
            printf("[DEBUG BINARY] %f > %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_LT: {
            double result = (left_val < right_val) ? 1.0 : 0.0;
            printf("[DEBUG BINARY] %f < %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_GTE: {
            double result = (left_val >= right_val) ? 1.0 : 0.0;
            printf("[DEBUG BINARY] %f >= %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_LTE: {
            double result = (left_val <= right_val) ? 1.0 : 0.0;
            printf("[DEBUG BINARY] %f <= %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_SPACESHIP: {
            // Opérateur spaceship (<=>)
            double result;
            if (left_val < right_val) result = -1.0;
            else if (left_val > right_val) result = 1.0;
            else result = 0.0;
            printf("[DEBUG BINARY] %f <=> %f = %f\n", left_val, right_val, result);
            return result;
        }
        
        // Opérateurs logiques (version numérique)
        case TK_AND: {
            double result = (left_val != 0.0 && right_val != 0.0) ? 1.0 : 0.0;
            printf("[DEBUG BINARY] %f && %f = %f\n", left_val, right_val, result);
            return result;
        }
        case TK_OR: {
            double result = (left_val != 0.0 || right_val != 0.0) ? 1.0 : 0.0;
            printf("[DEBUG BINARY] %f || %f = %f\n", left_val, right_val, result);
            return result;
        }
        
        // Opérateurs bitwise (convertis en entiers)
        case TK_BIT_AND: {
            int64_t left_int = (int64_t)left_val;
            int64_t right_int = (int64_t)right_val;
            double result = (double)(left_int & right_int);
            printf("[DEBUG BINARY] %lld & %lld = %lld\n", left_int, right_int, (int64_t)result);
            return result;
        }
        case TK_BIT_OR: {
            int64_t left_int = (int64_t)left_val;
            int64_t right_int = (int64_t)right_val;
            double result = (double)(left_int | right_int);
            printf("[DEBUG BINARY] %lld | %lld = %lld\n", left_int, right_int, (int64_t)result);
            return result;
        }
        case TK_BIT_XOR: {
            int64_t left_int = (int64_t)left_val;
            int64_t right_int = (int64_t)right_val;
            double result = (double)(left_int ^ right_int);
            printf("[DEBUG BINARY] %lld ^ %lld = %lld\n", left_int, right_int, (int64_t)result);
            return result;
        }
        case TK_SHL: {
            int64_t left_int = (int64_t)left_val;
            int64_t right_int = (int64_t)right_val;
            double result = (double)(left_int << right_int);
            printf("[DEBUG BINARY] %lld << %lld = %lld\n", left_int, right_int, (int64_t)result);
            return result;
        }
        case TK_SHR: {
            int64_t left_int = (int64_t)left_val;
            int64_t right_int = (int64_t)right_val;
            double result = (double)(left_int >> right_int);
            printf("[DEBUG BINARY] %lld >> %lld = %lld\n", left_int, right_int, (int64_t)result);
            return result;
        }
        case TK_USHR: {
            uint64_t left_uint = (uint64_t)left_val;
            uint64_t right_uint = (uint64_t)right_val;
            double result = (double)(left_uint >> right_uint);
            printf("[DEBUG BINARY] %llu >>> %llu = %llu\n", left_uint, right_uint, (uint64_t)result);
            return result;
        }
        
        // Opérateurs spéciaux
        case TK_IN: {
            // "in" operator - retourne 1.0 si left est dans right (pour les collections)
            printf("[DEBUG BINARY] in operator not fully implemented\n");
            return 0.0;
        }
        case TK_IS: {
            // "is" operator - comparaison de type
            printf("[DEBUG BINARY] is operator not fully implemented\n");
            return (left_val == right_val) ? 1.0 : 0.0;
        }
        case TK_ISNOT: {
            printf("[DEBUG BINARY] isnot operator not fully implemented\n");
            return (left_val != right_val) ? 1.0 : 0.0;
        }
        
        default: {
            printf("[DEBUG BINARY] ERROR: Unknown binary operator: %d\n", node->op_type);
            printf("[DEBUG BINARY] Operator names: TK_PLUS=%d, TK_MINUS=%d\n", 
                   TK_PLUS, TK_MINUS);
            return 0.0;
        }
    }
}
        case NODE_UNARY: {
            double operand = evalFloat(node->left);
            switch (node->op_type) {
                case TK_MINUS: return -operand;
                case TK_NOT: return operand == 0.0 ? 1.0 : 0.0;
                default: return operand;
            }
        }
        case NODE_FUNC_CALL: {
            Function* func = findFunction(node->data.name);
            if (func) {
                // Save current context
                Function* prev_func = current_function;
                current_function = func;
                pushScope();
                
                // Bind arguments to parameters
                if (func->param_names && node->left) {
                    ASTNode* arg = node->left;
                    for (int i = 0; i < func->param_count && arg; i++) {
                        if (func->param_names[i]) {
                            // Create variable for parameter
                            if (var_count < MAX_VARIABLES) {
                                Variable* var = &vars[var_count];
                                strncpy(var->name, func->param_names[i], sizeof(var->name) - 1);
                                var->type = TK_VAR;
                                var->scope_level = scope_level;
                                var->is_initialized = true;
                                
                                double val = evalFloat(arg);
                                var->is_float = true;
                                var->value.float_val = val;
                                var_count++;
                            }
                        }
                        arg = arg->right;
                    }
                }
                
                // Execute function body
                func->has_returned = false;
                func->return_value = 0;
                if (func->body) execute(func->body);
                
                // Restore context
                popScope();
                current_function = prev_func;
                
                return func->return_value;
            }
            
            printf("[ERROR] Function not found: %s\n", node->data.name);
            return 0.0;
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
            char* result = malloc(32);
            sprintf(result, "%lld", node->data.int_val);
            return result;
        }
        case NODE_FLOAT: {
            char* result = malloc(32);
            sprintf(result, "%g", node->data.float_val);
            return result;
        }
        case NODE_BOOL:
            return str_copy(node->data.bool_val ? "true" : "false");
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_string) return str_copy(vars[idx].value.str_val);
                if (vars[idx].is_float) {
                    char* result = malloc(32);
                    sprintf(result, "%g", vars[idx].value.float_val);
                    return result;
                }
                char* result = malloc(32);
                sprintf(result, "%lld", vars[idx].value.int_val);
                return result;
            }
            return str_copy("undefined");
        }
        case NODE_BINARY:
            if (node->op_type == TK_CONCAT) {
                char* left = evalString(node->left);
                char* right = evalString(node->right);
                char* result = malloc(strlen(left) + strlen(right) + 1);
                strcpy(result, left);
                strcat(result, right);
                free(left);
                free(right);
                return result;
            }
            // Fall through
        default: {
            double val = evalFloat(node);
            char* result = malloc(32);
            sprintf(result, "%g", val);
            return result;
        }
    }
}

static bool evalBool(ASTNode* node) {
    if (!node) return false;
    
    switch (node->type) {
        case NODE_BOOL:
            return node->data.bool_val;
        case NODE_INT:
            return node->data.int_val != 0;
        case NODE_FLOAT:
            return fabs(node->data.float_val) > 1e-10;
        case NODE_STRING:
            return node->data.str_val && strlen(node->data.str_val) > 0;
        case NODE_NULL:
        case NODE_UNDEFINED:
            return false;
        default:
            return evalFloat(node) != 0.0;
    }
}

// ======================================================
// [SECTION] CONTROL FLOW EXECUTION
// ======================================================
static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_BLOCK: {
            pushScope();
            ASTNode* stmt = node->left;
            while (stmt && !(current_function && current_function->has_returned)) {
                execute(stmt);
                stmt = stmt->right;
            }
            popScope();
            break;
        }
        
        case NODE_IF: {
            if (evalBool(node->left)) {
                execute(node->right);
            } else if (node->third) {
                execute(node->third);
            }
            break;
        }
        
        case NODE_WHILE: {
            while (evalBool(node->left) && !(current_function && current_function->has_returned)) {
                execute(node->right);
            }
            break;
        }
        
        case NODE_FOR: {
            if (node->data.loop.init) execute(node->data.loop.init);
            while (evalBool(node->data.loop.condition) && 
                   !(current_function && current_function->has_returned)) {
                execute(node->data.loop.body);
                if (node->data.loop.update) execute(node->data.loop.update);
            }
            break;
        }
        
        case NODE_FOR_IN: {
            // For-in loop implementation
            char* var_name = node->data.for_in.var_name;
            ASTNode* iterable = node->data.for_in.iterable;
            // TODO: Implement iteration
            break;
        }
        
        case NODE_SWITCH: {
            double value = evalFloat(node->data.switch_stmt.expr);
            ASTNode* cases = node->data.switch_stmt.cases;
            bool case_matched = false;
            
            while (cases && !case_matched) {
                if (cases->type == NODE_CASE) {
                    double case_val = evalFloat(cases->data.case_stmt.value);
                    if (value == case_val) {
                        case_matched = true;
                        execute(cases->data.case_stmt.body);
                    }
                }
                cases = cases->right;
            }
            
            if (!case_matched && node->data.switch_stmt.default_case) {
                execute(node->data.switch_stmt.default_case);
            }
            break;
        }
        
        case NODE_RETURN: {
            if (current_function) {
                current_function->has_returned = true;
                if (node->left) {
                    current_function->return_value = evalFloat(node->left);
                    char* str_val = evalString(node->left);
                    if (current_function->return_string) {
                        free(current_function->return_string);
                    }
                    current_function->return_string = str_val;
                }
            }
            break;
        }
        
        case NODE_BREAK:
        case NODE_CONTINUE:
            // TODO: Implement break/continue
            break;
        
        case NODE_TRY: {
            execute(node->data.try_catch.try_block);
            // TODO: Implement catch/finally
            break;
        }
        
        case NODE_THROW: {
            printf("[ERROR] Exception thrown: ");
            if (node->left) {
                char* msg = evalString(node->left);
                printf("%s\n", msg);
                free(msg);
            } else {
                printf("Unknown error\n");
            }
            break;
        }
        
        case NODE_VAR_DECL:
        case NODE_CONST_DECL: {
            if (var_count < MAX_VARIABLES) {
                Variable* var = &vars[var_count];
                strncpy(var->name, node->data.name, sizeof(var->name) - 1);
                var->type = (node->type == NODE_CONST_DECL) ? TK_CONST : TK_VAR;
                var->scope_level = scope_level;
                var->is_constant = (node->type == NODE_CONST_DECL);
                var->is_initialized = false;
                
                if (node->left) {
                    var->is_initialized = true;
                    double val = evalFloat(node->left);
                    var->is_float = true;
                    var->value.float_val = val;
                }
                
                var_count++;
            }
            break;
        }
        
        case NODE_ASSIGN: {
    printf("%s[EXEC DEBUG]%s Entering NODE_ASSIGN case\n", COLOR_YELLOW, COLOR_RESET);
    
    if (node->data.name) {
        printf("%s[EXEC DEBUG]%s Assigning to variable: %s\n", COLOR_YELLOW, COLOR_RESET, node->data.name);
        
        int idx = findVar(node->data.name);
        if (idx >= 0) {
            printf("%s[EXEC DEBUG]%s Found variable at index %d\n", COLOR_YELLOW, COLOR_RESET, idx);
            
            if (vars[idx].is_constant) {
                printf("%s[EXEC ERROR]%s Cannot assign to constant '%s'\n", COLOR_RED, COLOR_RESET, node->data.name);
                break;
            }
            
            if (node->left) {
                printf("%s[EXEC DEBUG]%s Evaluating right-hand expression\n", COLOR_YELLOW, COLOR_RESET);
                
                // Évaluer l'expression complète
                double new_value = evalFloat(node->left);
                
                printf("%s[EXEC DEBUG]%s New value: %f\n", COLOR_YELLOW, COLOR_RESET, new_value);
                
                // Mettre à jour la variable
                vars[idx].is_initialized = true;
                vars[idx].is_float = true;
                vars[idx].is_string = false;
                vars[idx].value.float_val = new_value;
                
                printf("%s[EXEC DEBUG]%s Assignment complete: %s = %f\n", 
                       COLOR_GREEN, COLOR_RESET, node->data.name, new_value);
            } else {
                printf("%s[EXEC ERROR]%s No expression to assign\n", COLOR_RED, COLOR_RESET);
            }
        } else {
            printf("%s[EXEC ERROR]%s Variable '%s' not found\n", COLOR_RED, COLOR_RESET, node->data.name);
            
            // Créer la variable si elle n'existe pas
            if (var_count < MAX_VARIABLES) {
                Variable* var = &vars[var_count];
                strncpy(var->name, node->data.name, sizeof(var->name) - 1);
                var->type = TK_VAR;
                var->scope_level = scope_level;
                var->is_constant = false;
                var->is_initialized = false;
                
                if (node->left) {
                    double new_value = evalFloat(node->left);
                    var->is_initialized = true;
                    var->is_float = true;
                    var->value.float_val = new_value;
                    
                    printf("%s[EXEC DEBUG]%s Created new variable: %s = %f\n", 
                           COLOR_GREEN, COLOR_RESET, node->data.name, new_value);
                }
                
                var_count++;
            }
        }
    } else {
        printf("%s[EXEC ERROR]%s No variable name in assignment\n", COLOR_RED, COLOR_RESET);
    }
    break;
}
        case NODE_COMPOUND_ASSIGN: {
    if (node->data.name) {
        int idx = findVar(node->data.name);
        if (idx >= 0) {
            if (vars[idx].is_constant) {
                printf("%s[EXEC ERROR]%s Cannot assign to constant '%s'\n", COLOR_RED, COLOR_RESET, node->data.name);
                break;
            }
            
            double current_value;
            if (vars[idx].is_float) {
                current_value = vars[idx].value.float_val;
            } else {
                current_value = (double)vars[idx].value.int_val;
            }
            
            double right_value = evalFloat(node->right);
            double new_value = current_value;
            
            switch (node->op_type) {
                case TK_PLUS_ASSIGN: new_value = current_value + right_value; break;
                case TK_MINUS_ASSIGN: new_value = current_value - right_value; break;
                case TK_MULT_ASSIGN: new_value = current_value * right_value; break;
                case TK_DIV_ASSIGN: new_value = current_value / right_value; break;
                default: break;
            }
            
            vars[idx].is_float = true;
            vars[idx].value.float_val = new_value;
            
            printf("%s[DEBUG]%s Compound assign: %s op=%d %f -> %f\n", 
                   COLOR_YELLOW, COLOR_RESET, node->data.name, node->op_type, current_value, new_value);
        }
    }
    break;
}
        case NODE_FUNC:
            // Already registered during parsing
            break;
        
        case NODE_FUNC_CALL:
            evalFloat(node);
            break;
        
        case NODE_PRINT: {
            ASTNode* arg = node->left;
            while (arg) {
                char* str = evalString(arg);
                printf("%s", str);
                free(str);
                arg = arg->right;
                if (arg) printf(" ");
            }
            printf("\n");
            break;
        }
        
        case NODE_IMPORT: {
            for (int i = 0; i < node->data.imports.module_count; i++) {
                char* module = node->data.imports.modules[i];
                char* from = node->data.imports.from_module;
                loadModule(module, from, NULL);
            }
            break;
        }
        
        case NODE_CLASS:
            // Already registered during parsing
            break;
        
        default:
            // Evaluate as expression
            evalFloat(node);
            break;
    }
}

// ======================================================
// [SECTION] MAIN EXECUTION
// ======================================================
static void run(const char* source, const char* filename) {
    initWorkingDir(filename);
    registerNativeFunctions();
    
    // Parse the source
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (!nodes) {
        printf("[ERROR] Parsing failed\n");
        return;
    }
    
    // First pass: register functions and classes
    for (int i = 0; i < count; i++) {
        if (nodes[i]->type == NODE_FUNC) {
            int param_count = 0;
            ASTNode* param = nodes[i]->left;
            while (param) {
                param_count++;
                param = param->right;
            }
            registerFunction(nodes[i]->data.name, nodes[i]->left, 
                           nodes[i]->right, param_count, NULL, false);
        }
        else if (nodes[i]->type == NODE_CLASS) {
            registerClass(nodes[i]->data.class_def.name, NULL,
                         nodes[i]->data.class_def.members, NULL, false);
        }
    }
    
    // Second pass: execute everything except function/class declarations
    for (int i = 0; i < count; i++) {
        if (nodes[i]->type != NODE_FUNC && nodes[i]->type != NODE_CLASS) {
            execute(nodes[i]);
        }
    }
    
    // Cleanup
    for (int i = 0; i < count; i++) {
        free(nodes[i]);
    }
    free(nodes);
    
    // Reset state for next run
    var_count = 0;
    scope_level = 0;
    scope_stack_ptr = 0;
}

// ======================================================
// [SECTION] FILE LOADING
// ======================================================
static char* loadFile(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("[ERROR] Cannot open file: %s\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    return source;
}

// ======================================================
// [SECTION] REPL
// ======================================================
static void repl() {
    printf("SwiftFlow REPL (type 'exit' to quit)\n");
    
    char line[4096];
    while (1) {
        printf("swift> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        if (strcmp(line, "clear") == 0) {
            system("clear");
            continue;
        }
        if (strlen(line) == 0) continue;
        
        run(line, "REPL");
    }
}

// ======================================================
// [SECTION] MAIN ENTRY POINT
// ======================================================
int main(int argc, char* argv[]) {
    if (argc < 2) {
        repl();
        return 0;
    }
    
    // Handle command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            printf("SwiftFlow 1.0.0\n");
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            printf("Usage: swift [file.swf] [options]\n");
            printf("Options:\n");
            printf("  -v, --version    Show version\n");
            printf("  -h, --help       Show this help\n");
            return 0;
        }
    }
    
    // Run the file
    char* source = loadFile(argv[1]);
    if (!source) return 1;
    
    run(source, argv[1]);
    free(source);
    
    return 0;
}

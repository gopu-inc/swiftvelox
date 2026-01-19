#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
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
    bool is_string;
    bool is_initialized;
    bool is_constant;
    int scope_level;
    char* module;  // Module d'origine
    bool is_exported;  // Si la variable est exportée
} Variable;

static Variable vars[500];
static int var_count = 0;
static int scope_level = 0;

// ======================================================
// [SECTION] DBVAR TABLE - STRUCTURE UNIQUE
// ======================================================
typedef struct {
    char name[100];
    char type[20];
    int size_bytes;
    char value_str[100];
    bool initialized;
    char module[100];
} DBVarEntry;

static DBVarEntry dbvars[500];
static int dbvar_count = 0;

// ======================================================
// [SECTION] PACKAGE SYSTEM
// ======================================================
typedef struct {
    char alias[100];
    char module_path[200];
    char package_name[100];
} ExportEntry;

typedef struct {
    char name[100];
    ExportEntry exports[50];
    int export_count;
    bool loaded;
} Package;

static Package packages[50];
static int package_count = 0;

static char* current_module = NULL;
static char* current_package = NULL;

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static int calculateVariableSize(TokenKind type) {
    switch (type) {
        case TK_VAR: return (rand() % 5) + 1;       // 1-5 bytes
        case TK_NET: return (rand() % 8) + 1;       // 1-8 bytes
        case TK_CLOG: return (rand() % 25) + 1;     // 1-25 bytes
        case TK_DOS: return (rand() % 1024) + 1;    // 1-1024 bytes
        case TK_SEL: return (rand() % 128) + 1;     // 1-128 bytes
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
        case TK_TYPE_INT: return "int";
        case TK_TYPE_FLOAT: return "float";
        case TK_TYPE_STR: return "string";
        case TK_TYPE_BOOL: return "bool";
        default: return "unknown";
    }
}

static int findVar(const char* name) {
    for (int i = var_count - 1; i >= 0; i--) {
        if (strcmp(vars[i].name, name) == 0 && vars[i].scope_level <= scope_level) {
            return i;
        }
    }
    return -1;
}

static void addToDBVar(const char* name, TokenKind type, int size_bytes, 
                       const char* value_str, bool initialized, const char* module) {
    if (dbvar_count < 500) {
        DBVarEntry* entry = &dbvars[dbvar_count++];
        strncpy(entry->name, name, 99);
        entry->name[99] = '\0';
        strncpy(entry->type, getTypeName(type), 19);
        entry->type[19] = '\0';
        entry->size_bytes = size_bytes;
        if (value_str) {
            strncpy(entry->value_str, value_str, 99);
            entry->value_str[99] = '\0';
        } else {
            strcpy(entry->value_str, "N/A");
        }
        entry->initialized = initialized;
        if (module) {
            strncpy(entry->module, module, 99);
            entry->module[99] = '\0';
        } else {
            strcpy(entry->module, current_module ? current_module : "global");
        }
    }
}

static void updateDBVar(const char* name, const char* value_str) {
    for (int i = 0; i < dbvar_count; i++) {
        if (strcmp(dbvars[i].name, name) == 0) {
            if (value_str) {
                strncpy(dbvars[i].value_str, value_str, 99);
                dbvars[i].value_str[99] = '\0';
            }
            dbvars[i].initialized = true;
            break;
        }
    }
}

// ======================================================
// [SECTION] PACKAGE MANAGEMENT
// ======================================================
static Package* findPackage(const char* name) {
    for (int i = 0; i < package_count; i++) {
        if (strcmp(packages[i].name, name) == 0) {
            return &packages[i];
        }
    }
    return NULL;
}

static Package* createPackage(const char* name) {
    if (package_count >= 50) {
        printf(RED "[ERROR]" RESET " Too many packages\n");
        return NULL;
    }
    
    Package* pkg = &packages[package_count++];
    strncpy(pkg->name, name, 99);
    pkg->name[99] = '\0';
    pkg->export_count = 0;
    pkg->loaded = false;
    
    return pkg;
}

static void addExportToPackage(const char* package_name, const char* alias, const char* module_path) {
    Package* pkg = findPackage(package_name);
    if (!pkg) {
        pkg = createPackage(package_name);
        if (!pkg) return;
    }
    
    if (pkg->export_count >= 50) {
        printf(RED "[ERROR]" RESET " Too many exports in package %s\n", package_name);
        return;
    }
    
    ExportEntry* exp = &pkg->exports[pkg->export_count++];
    strncpy(exp->alias, alias, 99);
    exp->alias[99] = '\0';
    strncpy(exp->module_path, module_path, 199);
    exp->module_path[199] = '\0';
    strncpy(exp->package_name, package_name, 99);
    exp->package_name[99] = '\0';
    
    printf(CYAN "[PACKAGE]" RESET " %s exports '%s' as '%s'\n", 
           package_name, module_path, alias);
}

static char* findModulePath(const char* module_name, const char* from_package) {
    char* path = NULL;
    
    // 1. Si from_package est spécifié, chercher dans le package
    if (from_package) {
        Package* pkg = findPackage(from_package);
        if (pkg) {
            for (int i = 0; i < pkg->export_count; i++) {
                if (strcmp(pkg->exports[i].alias, module_name) == 0) {
                    // Construire le chemin complet
                    path = malloc(strlen("/usr/local/lib/swift/") + 
                                 strlen(pkg->exports[i].package_name) + 
                                 strlen(pkg->exports[i].module_path) + 10);
                    if (path) {
                        sprintf(path, "/usr/local/lib/swift/%s/%s", 
                                pkg->exports[i].package_name, pkg->exports[i].module_path);
                        printf(CYAN "[RESOLVE]" RESET " Found export: %s -> %s\n", module_name, path);
                        return path;
                    }
                }
            }
        }
    }
    
    // 2. Chercher dans le répertoire courant
    if (!path) {
        struct stat st;
        // Essayer .swf
        char local_path[300];
        sprintf(local_path, "%s.swf", module_name);
        if (stat(local_path, &st) == 0) {
            path = strdup(local_path);
            printf(CYAN "[RESOLVE]" RESET " Found local module: %s\n", path);
            return path;
        }
        
        // Essayer sans extension
        if (stat(module_name, &st) == 0) {
            path = strdup(module_name);
            return path;
        }
    }
    
    // 3. Chercher dans /usr/local/lib/swift/
    if (!path) {
        char* base_path = "/usr/local/lib/swift/";
        
        // Si c'est un package (dossier)
        char package_dir[300];
        sprintf(package_dir, "%s%s/", base_path, module_name);
        
        struct stat st;
        if (stat(package_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
            // Vérifier s'il y a un fichier .svlib
            char svlib_path[350];
            sprintf(svlib_path, "%s%s.svlib", package_dir, module_name);
            
            if (access(svlib_path, F_OK) == 0) {
                path = strdup(svlib_path);
                printf(CYAN "[PACKAGE]" RESET " Found package manifest: %s\n", path);
                return path;
            }
            
            // Sinon, chercher un fichier .swf avec le même nom
            sprintf(svlib_path, "%s%s.swf", package_dir, module_name);
            if (access(svlib_path, F_OK) == 0) {
                path = strdup(svlib_path);
                return path;
            }
        }
        
        // Essayer fichier simple
        char module_path[300];
        sprintf(module_path, "%s%s.swf", base_path, module_name);
        if (access(module_path, F_OK) == 0) {
            path = strdup(module_path);
            printf(CYAN "[RESOLVE]" RESET " Found system module: %s\n", path);
            return path;
        }
    }
    
    // 4. Si current_package est défini, chercher dedans
    if (!path && current_package) {
        char package_path[300];
        sprintf(package_path, "/usr/local/lib/swift/%s/%s", current_package, module_name);
        
        if (access(package_path, F_OK) == 0) {
            path = strdup(package_path);
            return path;
        }
        
        sprintf(package_path, "/usr/local/lib/swift/%s/%s.swf", current_package, module_name);
        if (access(package_path, F_OK) == 0) {
            path = strdup(package_path);
            return path;
        }
    }
    
    printf(RED "[RESOLVE]" RESET " Module not found: %s\n", module_name);
    return NULL;
}

static void loadPackage(const char* package_name) {
    Package* pkg = findPackage(package_name);
    if (pkg && pkg->loaded) {
        return; // Déjà chargé
    }
    
    char* manifest_path = findModulePath(package_name, NULL);
    if (!manifest_path) {
        printf(RED "[ERROR]" RESET " Cannot find package manifest for: %s\n", package_name);
        return;
    }
    
    // Vérifier si c'est un fichier .svlib
    const char* ext = strrchr(manifest_path, '.');
    if (!ext || strcmp(ext, ".svlib") != 0) {
        printf(RED "[ERROR]" RESET " Not a package manifest: %s\n", manifest_path);
        free(manifest_path);
        return;
    }
    
    FILE* f = fopen(manifest_path, "r");
    if (!f) {
        printf(RED "[ERROR]" RESET " Cannot open package manifest: %s\n", manifest_path);
        free(manifest_path);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    printf(CYAN "[PACKAGE]" RESET " Loading package: %s\n", package_name);
    
    // Sauvegarder l'état courant
    char* old_package = current_package ? strdup(current_package) : NULL;
    char* old_module = current_module ? strdup(current_module) : NULL;
    
    // Définir le package courant
    current_package = strdup(package_name);
    current_module = strdup(package_name);
    
    // Exécuter le manifest
    int node_count = 0;
    ASTNode** nodes = parse(source, &node_count);
    
    if (nodes) {
        for (int i = 0; i < node_count; i++) {
            // Le manifest peut contenir des exports
            if (nodes[i]) {
                if (nodes[i]->type == NODE_EXPORT) {
                    addExportToPackage(package_name, 
                                      nodes[i]->data.export.alias,
                                      nodes[i]->data.export.symbol);
                }
                free(nodes[i]);
            }
        }
        free(nodes);
    }
    
    // Marquer comme chargé
    if (pkg) {
        pkg->loaded = true;
    } else {
        pkg = createPackage(package_name);
        if (pkg) pkg->loaded = true;
    }
    
    // Restaurer l'état
    free(current_package);
    free(current_module);
    current_package = old_package;
    current_module = old_module;
    
    free(source);
    free(manifest_path);
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
                } else if (vars[idx].is_string) {
                    return 0.0;
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
                case TK_DIV: return right != 0 ? left / right : 0.0;
                case TK_MOD: return right != 0 ? fmod(left, right) : 0.0;
                case TK_EQ: return left == right ? 1.0 : 0.0;
                case TK_NEQ: return left != right ? 1.0 : 0.0;
                case TK_GT: return left > right ? 1.0 : 0.0;
                case TK_LT: return left < right ? 1.0 : 0.0;
                case TK_GTE: return left >= right ? 1.0 : 0.0;
                case TK_LTE: return left <= right ? 1.0 : 0.0;
                default: return 0.0;
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
            
        case NODE_INT:
            result = malloc(32);
            sprintf(result, "%lld", node->data.int_val);
            break;
            
        case NODE_FLOAT:
            result = malloc(32);
            sprintf(result, "%.2f", node->data.float_val);
            break;
            
        case NODE_BOOL:
            result = str_copy(node->data.bool_val ? "true" : "false");
            break;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_string && vars[idx].value.str_val) {
                    result = str_copy(vars[idx].value.str_val);
                } else if (vars[idx].is_float) {
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
            
        case NODE_BINARY: {
            if (node->op_type == TK_CONCAT) {
                char* left_str = evalString(node->left);
                char* right_str = evalString(node->right);
                result = malloc(strlen(left_str) + strlen(right_str) + 1);
                if (result) {
                    strcpy(result, left_str);
                    strcat(result, right_str);
                }
                free(left_str);
                free(right_str);
            } else {
                double val = evalFloat(node);
                result = malloc(32);
                sprintf(result, "%.2f", val);
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
// [SECTION] MODULE IMPORT
// ======================================================
static void importModule(const char* module_name, const char* from_package);

static void executeImport(ASTNode* node) {
    if (!node) return;
    
    for (int i = 0; i < node->data.imports.module_count; i++) {
        if (node->data.imports.modules[i]) {
            importModule(node->data.imports.modules[i], node->data.imports.from_module);
        }
    }
}

static void importModule(const char* module_name, const char* from_package) {
    printf(CYAN "[IMPORT]" RESET " Importing: %s", module_name);
    if (from_package) printf(" from package: %s", from_package);
    printf("\n");
    
    // Vérifier si c'est un wildcard
    if (strcmp(module_name, "*") == 0 && from_package) {
        // Charger tout le package
        loadPackage(from_package);
        return;
    }
    
    // Charger le package si nécessaire
    if (from_package) {
        loadPackage(from_package);
    }
    
    // Trouver le chemin du module
    char* module_path = findModulePath(module_name, from_package);
    if (!module_path) {
        printf(RED "[ERROR]" RESET " Cannot find module: %s\n", module_name);
        return;
    }
    
    // Vérifier si c'est un package manifest
    const char* ext = strrchr(module_path, '.');
    if (ext && strcmp(ext, ".svlib") == 0) {
        // C'est un package, le charger
        loadPackage(module_name);
        free(module_path);
        return;
    }
    
    // Ouvrir et exécuter le module
    FILE* f = fopen(module_path, "r");
    if (!f) {
        printf(RED "[ERROR]" RESET " Cannot open module: %s\n", module_path);
        free(module_path);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    // Sauvegarder l'état
    char* old_module = current_module ? strdup(current_module) : NULL;
    char* old_package = current_package ? strdup(current_package) : NULL;
    
    // Définir le module courant
    current_module = strdup(module_name);
    
    // Extraire le nom du package du chemin si possible
    char* path_copy = strdup(module_path);
    char* last_slash = strrchr(path_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        char* package_slash = strrchr(path_copy, '/');
        if (package_slash) {
            current_package = strdup(package_slash + 1);
        }
    }
    free(path_copy);
    
    // Exécuter le module
    printf(CYAN "[MODULE]" RESET " Executing module: %s\n", module_name);
    
    int node_count = 0;
    ASTNode** nodes = parse(source, &node_count);
    
    if (nodes) {
        for (int i = 0; i < node_count; i++) {
            if (nodes[i]) {
                // Éviter de réexécuter les imports dans le module importé
                // pour éviter les boucles infinies
                if (nodes[i]->type == NODE_IMPORT) {
                    // Marquer les variables exportées
                    printf(CYAN "[MODULE]" RESET " Skipping imports in imported module\n");
                } else {
                    // Exécuter normalement
                    // (Note: l'exécution complète nécessiterait une fonction execute() séparée)
                }
                free(nodes[i]);
            }
        }
        free(nodes);
    }
    
    // Restaurer l'état
    free(current_module);
    free(current_package);
    current_module = old_module;
    current_package = old_package;
    
    free(source);
    free(module_path);
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
            
            if (var_count < 500) {
                Variable* var = &vars[var_count];
                strncpy(var->name, node->data.name, 99);
                var->name[99] = '\0';
                var->type = var_type;
                var->size_bytes = calculateVariableSize(var_type);
                var->scope_level = scope_level;
                var->is_constant = (var_type == TK_CONST);
                var->module = current_module ? strdup(current_module) : NULL;
                var->is_exported = false; // Par défaut
                
                if (node->left) {
                    var->is_initialized = true;
                    
                    if (node->left->type == NODE_STRING) {
                        var->is_string = true;
                        var->is_float = false;
                        var->value.str_val = str_copy(node->left->data.str_val);
                        addToDBVar(var->name, var_type, var->size_bytes, 
                                  var->value.str_val, true, var->module);
                        printf(GREEN "[DECL]" RESET " %s %s = \"%s\" (%d bytes) [%s]\n", 
                               getTypeName(var_type), var->name, var->value.str_val, 
                               var->size_bytes, var->module ? var->module : "global");
                    } 
                    else if (node->left->type == NODE_FLOAT) {
                        var->is_float = true;
                        var->is_string = false;
                        var->value.float_val = evalFloat(node->left);
                        
                        char value_str[32];
                        sprintf(value_str, "%.2f", var->value.float_val);
                        addToDBVar(var->name, var_type, var->size_bytes, value_str, true, var->module);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = %.2f (%d bytes) [%s]\n", 
                               getTypeName(var_type), var->name, var->value.float_val, 
                               var->size_bytes, var->module ? var->module : "global");
                    }
                    else if (node->left->type == NODE_BOOL) {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = node->left->data.bool_val ? 1 : 0;
                        
                        addToDBVar(var->name, var_type, var->size_bytes, 
                                  node->left->data.bool_val ? "true" : "false", true, var->module);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = %s (%d bytes) [%s]\n", 
                               getTypeName(var_type), var->name, 
                               node->left->data.bool_val ? "true" : "false", 
                               var->size_bytes, var->module ? var->module : "global");
                    }
                    else {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = (int64_t)evalFloat(node->left);
                        
                        char value_str[32];
                        sprintf(value_str, "%lld", var->value.int_val);
                        addToDBVar(var->name, var_type, var->size_bytes, value_str, true, var->module);
                        
                        printf(GREEN "[DECL]" RESET " %s %s = %lld (%d bytes) [%s]\n", 
                               getTypeName(var_type), var->name, var->value.int_val, 
                               var->size_bytes, var->module ? var->module : "global");
                    }
                } else {
                    var->is_initialized = false;
                    var->is_float = false;
                    var->is_string = false;
                    var->value.int_val = 0;
                    
                    addToDBVar(var->name, var_type, var->size_bytes, "uninitialized", false, var->module);
                    
                    printf(GREEN "[DECL]" RESET " %s %s (uninitialized, %d bytes) [%s]\n", 
                           getTypeName(var_type), var->name, var->size_bytes, 
                           var->module ? var->module : "global");
                }
                
                var_count++;
            }
            break;
        }
            
        case NODE_ASSIGN: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_constant) {
                    printf(RED "[ERROR]" RESET " Cannot assign to constant '%s'\n", node->data.name);
                    return;
                }
                
                if (node->left) {
                    vars[idx].is_initialized = true;
                    
                    if (node->left->type == NODE_STRING) {
                        if (vars[idx].value.str_val) free(vars[idx].value.str_val);
                        vars[idx].is_string = true;
                        vars[idx].is_float = false;
                        vars[idx].value.str_val = str_copy(node->left->data.str_val);
                        updateDBVar(node->data.name, vars[idx].value.str_val);
                    }
                    else if (node->left->type == NODE_FLOAT) {
                        vars[idx].is_float = true;
                        vars[idx].is_string = false;
                        vars[idx].value.float_val = evalFloat(node->left);
                        
                        char value_str[32];
                        sprintf(value_str, "%.2f", vars[idx].value.float_val);
                        updateDBVar(node->data.name, value_str);
                    }
                    else if (node->left->type == NODE_BOOL) {
                        vars[idx].is_float = false;
                        vars[idx].is_string = false;
                        vars[idx].value.int_val = node->left->data.bool_val ? 1 : 0;
                        
                        updateDBVar(node->data.name, node->left->data.bool_val ? "true" : "false");
                    }
                    else {
                        vars[idx].is_float = false;
                        vars[idx].is_string = false;
                        vars[idx].value.int_val = (int64_t)evalFloat(node->left);
                        
                        char value_str[32];
                        sprintf(value_str, "%lld", vars[idx].value.int_val);
                        updateDBVar(node->data.name, value_str);
                    }
                    
                    printf(CYAN "[ASSIGN]" RESET " %s = ", node->data.name);
                    if (vars[idx].is_string) {
                        printf("\"%s\"\n", vars[idx].value.str_val);
                    } else if (vars[idx].is_float) {
                        printf("%.2f\n", vars[idx].value.float_val);
                    } else {
                        printf("%lld\n", vars[idx].value.int_val);
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
                printf("%d bytes", vars[idx].size_bytes);
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", 
                       node->data.size_info.var_name);
            }
            break;
        }
            
        case NODE_DBVAR: {
            printf(CYAN "\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
            printf("║                   TABLE DES VARIABLES (dbvar)                              ║\n");
            printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
            printf("║  Type    │ Nom          │ Taille     │ Valeur         │ Init  │ Module     ║\n");
            printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < dbvar_count; i++) {
                DBVarEntry* entry = &dbvars[i];
                printf("║ %-8s │ %-12s │ %-10d │ %-14s │ %-5s │ %-10s ║\n",
                       entry->type,
                       entry->name,
                       entry->size_bytes,
                       entry->value_str,
                       entry->initialized ? "✓" : "✗",
                       entry->module);
            }
            
            if (dbvar_count == 0) {
                printf("║                      No variables declared                                    ║\n");
            }
            
            printf("╚══════════════════════════════════════════════════════════════════════════════╝\n" RESET);
            break;
        }
            
        case NODE_IMPORT:
            executeImport(node);
            break;
            
        case NODE_EXPORT: {
            // Marquer la variable comme exportée
            int idx = findVar(node->data.export.symbol);
            if (idx >= 0) {
                vars[idx].is_exported = true;
                printf(CYAN "[EXPORT]" RESET " Exporting variable: %s as %s\n", 
                       node->data.export.symbol, node->data.export.alias);
                
                // Ajouter à la table d'export du package courant
                if (current_package) {
                    addExportToPackage(current_package, 
                                      node->data.export.alias,
                                      node->data.export.symbol);
                }
            } else {
                printf(RED "[ERROR]" RESET " Cannot export undefined variable: %s\n", 
                       node->data.export.symbol);
            }
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
            ASTNode* current_node = node->left;
            while (current_node) {
                execute(current_node);
                current_node = current_node->right;
            }
            scope_level--;
            break;
        }
            
        case NODE_MAIN: {
            printf(CYAN "\n[EXEC]" RESET " Starting main()...\n");
            for (int i = 0; i < 60; i++) printf("=");
            printf("\n");
            
            if (node->left) execute(node->left);
            
            printf("\n" CYAN "[EXEC]" RESET " main() finished successfully\n");
            break;
        }
            
        case NODE_RETURN: {
            if (node->left) {
                char* str = evalString(node->left);
                printf(CYAN "[RETURN]" RESET " %s\n", str);
                free(str);
            } else {
                printf(CYAN "[RETURN]" RESET " (no value)\n");
            }
            break;
        }
            
        case NODE_BINARY: {
            // Évaluation directe d'une expression binaire
            double result = evalFloat(node);
            printf("%.2f\n", result);
            break;
        }
            
        default:
            break;
    }
}

// ======================================================
// [SECTION] CLEANUP FUNCTIONS
// ======================================================
static void cleanupVariables() {
    for (int i = 0; i < var_count; i++) {
        if (vars[i].is_string && vars[i].value.str_val) {
            free(vars[i].value.str_val);
        }
        if (vars[i].module) free(vars[i].module);
    }
    var_count = 0;
    scope_level = 0;
}

static void cleanupDBVar() {
    dbvar_count = 0;
}

static void cleanupPackages() {
    package_count = 0;
    if (current_module) free(current_module);
    if (current_package) free(current_package);
    current_module = NULL;
    current_package = NULL;
}

// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void run(const char* source, const char* filename) {
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (!nodes) {
        printf(RED "[ERROR]" RESET " Failed to parse source code\n");
        return;
    }
    
    // Set current module name
    if (current_module) free(current_module);
    current_module = strdup(filename);
    
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
    
    // Cleanup des nœuds AST
    for (int i = 0; i < count; i++) {
        if (nodes[i]) {
            if (nodes[i]->type == NODE_IMPORT) {
                for (int j = 0; j < nodes[i]->data.imports.module_count; j++) {
                    free(nodes[i]->data.imports.modules[j]);
                }
                free(nodes[i]->data.imports.modules);
                if (nodes[i]->data.imports.from_module) {
                    free(nodes[i]->data.imports.from_module);
                }
            }
            if (nodes[i]->type == NODE_EXPORT) {
                free(nodes[i]->data.export.symbol);
                free(nodes[i]->data.export.alias);
            }
            if (nodes[i]->type == NODE_STRING && nodes[i]->data.str_val) {
                free(nodes[i]->data.str_val);
            }
            if (nodes[i]->type == NODE_IDENT && nodes[i]->data.name) {
                free(nodes[i]->data.name);
            }
            if (nodes[i]->type == NODE_SIZEOF && nodes[i]->data.size_info.var_name) {
                free(nodes[i]->data.size_info.var_name);
            }
            free(nodes[i]);
        }
    }
    free(nodes);
}

// ======================================================
// [SECTION] REPL
// ======================================================
static void repl() {
    printf(GREEN "╔════════════════════════════════════════════════════════════════════╗\n");
    printf("║                SwiftFlow v2.0 - REPL Mode                        ║\n");
    printf("║           Types CLAIR & SYM Fusion - Complete Language           ║\n");
    printf("╚════════════════════════════════════════════════════════════════════╝\n" RESET);
    printf("\n");
    printf("Available commands:\n");
    printf("  " CYAN "exit" RESET "      - Quit the REPL\n");
    printf("  " CYAN "dbvar" RESET "     - Show variable table\n");
    printf("  " CYAN "clear" RESET "     - Clear screen\n");
    printf("  " CYAN "reset" RESET "     - Reset all variables\n");
    printf("  " CYAN "packages" RESET "  - Show loaded packages\n");
    printf("\n");
    printf("Example statements:\n");
    printf("  " YELLOW "var x = 10;" RESET "\n");
    printf("  " YELLOW "net y = 20.5;" RESET "\n");
    printf("  " YELLOW "print(x + y);" RESET "\n");
    printf("  " YELLOW "if (x > 5) print(\"x is big\");" RESET "\n");
    printf("  " YELLOW "import \"math\" from \"stdlib\";" RESET "\n");
    printf("  " YELLOW "export myVar as \"publicVar\";" RESET "\n");
    printf("\n");
    
    char line[1024];
    while (1) {
        printf(GREEN "swift> " RESET);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0) break;
        if (strcmp(line, "clear") == 0) {
            system("clear");
            continue;
        }
        if (strcmp(line, "dbvar") == 0) {
            printf(CYAN "\n╔══════════════════════════════════════════════════════════════════════════════╗\n");
            printf("║                   TABLE DES VARIABLES (dbvar)                              ║\n");
            printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
            printf("║  Type    │ Nom          │ Taille     │ Valeur         │ Init  │ Module     ║\n");
            printf("╠══════════════════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < dbvar_count; i++) {
                DBVarEntry* entry = &dbvars[i];
                printf("║ %-8s │ %-12s │ %-10d │ %-14s │ %-5s │ %-10s ║\n",
                       entry->type,
                       entry->name,
                       entry->size_bytes,
                       entry->value_str,
                       entry->initialized ? "✓" : "✗",
                       entry->module);
            }
            
            if (dbvar_count == 0) {
                printf("║                      No variables declared                                    ║\n");
            }
            
            printf("╚══════════════════════════════════════════════════════════════════════════════╝\n" RESET);
            continue;
        }
        if (strcmp(line, "packages") == 0) {
            printf(CYAN "\n╔════════════════════════════════════════════════════════════════════╗\n");
            printf("║                    LOADED PACKAGES                                ║\n");
            printf("╠════════════════════════════════════════════════════════════════════╣\n");
            
            for (int i = 0; i < package_count; i++) {
                Package* pkg = &packages[i];
                printf("║ %-20s │ Exports: %-3d │ Loaded: %-5s ║\n",
                       pkg->name,
                       pkg->export_count,
                       pkg->loaded ? "✓" : "✗");
                
                for (int j = 0; j < pkg->export_count; j++) {
                    printf("║   └─ %-15s → %-30s ║\n",
                           pkg->exports[j].alias,
                           pkg->exports[j].module_path);
                }
            }
            
            if (package_count == 0) {
                printf("║                 No packages loaded                              ║\n");
            }
            
            printf("╚════════════════════════════════════════════════════════════════════╝\n" RESET);
            continue;
        }
        if (strcmp(line, "reset") == 0) {
            cleanupVariables();
            cleanupDBVar();
            cleanupPackages();
            printf(GREEN "[INFO]" RESET " All variables and packages reset\n");
            continue;
        }
        if (strlen(line) == 0) continue;
        
        run(line, "REPL");
    }
    
    printf(CYAN "\n[REPL]" RESET " Exiting SwiftFlow. Goodbye!\n");
}

// ======================================================
// [SECTION] FILE LOADING
// ======================================================
static char* loadFile(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf(RED "[ERROR]" RESET " Cannot open file: %s\n", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        printf(RED "[ERROR]" RESET " Memory allocation failed\n");
        return NULL;
    }
    
    size_t bytes_read = fread(source, 1, size, f);
    source[bytes_read] = '\0';
    fclose(f);
    
    return source;
}

// ======================================================
// [SECTION] MAIN
// ======================================================
int main(int argc, char* argv[]) {
    // Initialisation
    srand(time(NULL));
    
    printf(CYAN "\n");
    printf("   _____ _       __ _   _____ _                 \n");
    printf("  / ____(_)     / _| | |  __ (_)                \n");
    printf(" | (___  _ _ __| |_| |_| |__) | _____      __   \n");
    printf("  \\___ \\| | '__|  _| __|  ___/ |/ _ \\ \\ /\\ / /  \n");
    printf("  ____) | | |  | | | |_| |   | | (_) \\ V  V /   \n");
    printf(" |_____/|_|_|  |_|  \\__|_|   |_|\\___/ \\_/\\_/    \n");
    printf("                                                \n");
    printf("         v2.0 - Fusion CLAIR & SYM              \n");
    printf("         Advanced Package System                \n");
    printf(RESET "\n");
    
    if (argc < 2) {
        // Mode REPL
        repl();
    } else {
        // Mode fichier
        char* source = loadFile(argv[1]);
        if (!source) {
            return 1;
        }
        
        printf(GREEN "[SUCCESS]" RESET ">> Executing %s\n", argv[1]);
        for (int i = 0; i < 60; i++) printf("=");
        printf("\n\n");
        
        run(source, argv[1]);
        free(source);
    }
    
    // Nettoyage final
    cleanupVariables();
    cleanupDBVar();
    cleanupPackages();
    
    return 0;
}

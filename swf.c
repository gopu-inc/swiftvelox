#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <pthread.h>
#include "common.h"

// ======================================================
// [SECTION] FORWARD DECLARATIONS
// ======================================================
static Value* evaluate(ASTNode* node, Error* error);
static Value* eval_expression(ASTNode* node, Error* error);
static Value* eval_binary(ASTNode* node, Error* error);
static Value* eval_unary(ASTNode* node, Error* error);
static Value* eval_call(ASTNode* node, Error* error);
static Value* eval_member_access(ASTNode* node, Error* error);
static Value* eval_new(ASTNode* node, Error* error);
static Value* eval_array(ASTNode* node, Error* error);
static Value* eval_map(ASTNode* node, Error* error);

// ======================================================
// [SECTION] VM STATE - GLOBAL
// ======================================================
static Scope* global_scope = NULL;
static Scope* current_scope = NULL;
static ClassDefinition* class_list = NULL;
static Package* package_list = NULL;
static char* current_module = NULL;
static char* current_package = NULL;
static char* current_dir = NULL;
static bool in_repl_mode = false;
static int next_var_id = 1;

// ======================================================
// [SECTION] DBVAR TABLE
// ======================================================
typedef struct {
    int id;
    char name[256];
    char type[32];
    int size_bytes;
    char value_str[256];
    bool initialized;
    char module[256];
    int scope_level;
    bool is_constant;
    bool is_exported;
} DBVarEntry;

static DBVarEntry dbvars[1000];
static int dbvar_count = 0;

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static Scope* create_scope(int level) {
    Scope* scope = malloc(sizeof(Scope));
    if (scope) {
        scope->variables = NULL;
        scope->level = level;
        scope->parent = NULL;
        scope->child = NULL;
    }
    return scope;
}

static void enter_scope() {
    Scope* new_scope = create_scope(current_scope ? current_scope->level + 1 : 0);
    if (new_scope) {
        new_scope->parent = current_scope;
        if (current_scope) {
            current_scope->child = new_scope;
        }
        current_scope = new_scope;
    }
}

static void exit_scope() {
    if (current_scope && current_scope->parent) {
        // Free variables in current scope
        Variable* var = current_scope->variables;
        while (var) {
            Variable* next = var->next;
            
            // Remove from dbvar table
            for (int i = 0; i < dbvar_count; i++) {
                if (strcmp(dbvars[i].name, var->name) == 0 && dbvars[i].scope_level == current_scope->level) {
                    // Shift remaining entries
                    for (int j = i; j < dbvar_count - 1; j++) {
                        dbvars[j] = dbvars[j + 1];
                    }
                    dbvar_count--;
                    break;
                }
            }
            
            if (var->value) {
                free_value(var->value);
            }
            free(var->name);
            free(var->module);
            free(var);
            var = next;
        }
        
        Scope* old_scope = current_scope;
        current_scope = current_scope->parent;
        current_scope->child = NULL;
        free(old_scope);
    }
}

static Variable* find_variable(const char* name) {
    Scope* scope = current_scope;
    while (scope) {
        Variable* var = scope->variables;
        while (var) {
            if (strcmp(var->name, name) == 0) {
                return var;
            }
            var = var->next;
        }
        scope = scope->parent;
    }
    return NULL;
}

static Variable* create_variable(const char* name, ValueType type) {
    Variable* var = malloc(sizeof(Variable));
    if (var) {
        strncpy(var->name, name, sizeof(var->name) - 1);
        var->name[sizeof(var->name) - 1] = '\0';
        var->value = create_value(type);
        var->declared_type = type;
        var->is_constant = false;
        var->is_mutable = true;
        var->scope_level = current_scope ? current_scope->level : 0;
        var->module = current_module ? str_copy(current_module) : NULL;
        var->is_exported = false;
        var->next = NULL;
        
        // Add to scope
        if (current_scope) {
            var->next = current_scope->variables;
            current_scope->variables = var;
        }
    }
    return var;
}

static void add_to_dbvar(Variable* var, const char* value_str) {
    if (dbvar_count >= 1000) return;
    
    DBVarEntry* entry = &dbvars[dbvar_count++];
    entry->id = next_var_id++;
    strncpy(entry->name, var->name, sizeof(entry->name) - 1);
    entry->name[sizeof(entry->name) - 1] = '\0';
    
    // Determine type name
    switch (var->declared_type) {
        case TYPE_INT: strcpy(entry->type, "int"); break;
        case TYPE_FLOAT: strcpy(entry->type, "float"); break;
        case TYPE_STRING: strcpy(entry->type, "string"); break;
        case TYPE_BOOL: strcpy(entry->type, "bool"); break;
        case TYPE_CHAR: strcpy(entry->type, "char"); break;
        case TYPE_VOID: strcpy(entry->type, "void"); break;
        case TYPE_ARRAY: strcpy(entry->type, "array"); break;
        case TYPE_MAP: strcpy(entry->type, "map"); break;
        case TYPE_OBJECT: strcpy(entry->type, "object"); break;
        case TYPE_CLASS: strcpy(entry->type, "class"); break;
        case TYPE_NULL: strcpy(entry->type, "null"); break;
        case TYPE_UNDEFINED: strcpy(entry->type, "undefined"); break;
        default: strcpy(entry->type, "unknown"); break;
    }
    
    // Calculate size
    entry->size_bytes = 0;
    if (var->value) {
        switch (var->value->type) {
            case TYPE_INT: entry->size_bytes = sizeof(int64_t); break;
            case TYPE_FLOAT: entry->size_bytes = sizeof(double); break;
            case TYPE_STRING: 
                if (var->value->as.str_val) {
                    entry->size_bytes = strlen(var->value->as.str_val) + 1;
                }
                break;
            case TYPE_BOOL: entry->size_bytes = sizeof(bool); break;
            case TYPE_CHAR: entry->size_bytes = sizeof(char); break;
            case TYPE_ARRAY:
                entry->size_bytes = var->value->as.array.length * sizeof(Value*);
                break;
            case TYPE_MAP:
                entry->size_bytes = var->value->as.map.count * (sizeof(char*) + sizeof(Value*));
                break;
            default: entry->size_bytes = sizeof(void*); break;
        }
    }
    
    if (value_str) {
        strncpy(entry->value_str, value_str, sizeof(entry->value_str) - 1);
        entry->value_str[sizeof(entry->value_str) - 1] = '\0';
    } else {
        strcpy(entry->value_str, "uninitialized");
    }
    
    entry->initialized = (var->value != NULL);
    entry->scope_level = var->scope_level;
    entry->is_constant = var->is_constant;
    entry->is_exported = var->is_exported;
    
    if (var->module) {
        strncpy(entry->module, var->module, sizeof(entry->module) - 1);
        entry->module[sizeof(entry->module) - 1] = '\0';
    } else {
        strcpy(entry->module, "global");
    }
}

static void update_dbvar(Variable* var, const char* value_str) {
    for (int i = 0; i < dbvar_count; i++) {
        if (strcmp(dbvars[i].name, var->name) == 0 && dbvars[i].scope_level == var->scope_level) {
            if (value_str) {
                strncpy(dbvars[i].value_str, value_str, sizeof(dbvars[i].value_str) - 1);
                dbvars[i].value_str[sizeof(dbvars[i].value_str) - 1] = '\0';
            }
            dbvars[i].initialized = true;
            break;
        }
    }
}

// ======================================================
// [SECTION] PATH RESOLUTION FOR PACKAGES
// ======================================================
static char* resolve_relative_path(const char* base_dir, const char* relative_path) {
    if (!relative_path || strlen(relative_path) == 0) {
        return NULL;
    }
    
    // Si le chemin commence par /, c'est absolu
    if (relative_path[0] == '/') {
        return str_copy(relative_path);
    }
    
    // Normaliser le chemin de base
    char* normalized_base = NULL;
    if (base_dir && strlen(base_dir) > 0) {
        normalized_base = str_copy(base_dir);
    } else {
        normalized_base = str_copy(".");
    }
    
    // Construire le chemin complet
    char* result = malloc(strlen(normalized_base) + strlen(relative_path) + 10);
    if (!result) {
        free(normalized_base);
        return NULL;
    }
    
    if (strcmp(relative_path, ".") == 0) {
        strcpy(result, normalized_base);
    } else if (strcmp(relative_path, "..") == 0) {
        // Remonter d'un niveau
        char* last_slash = strrchr(normalized_base, '/');
        if (last_slash) {
            *last_slash = '\0';
            strcpy(result, normalized_base);
        } else {
            strcpy(result, ".");
        }
    } else if (strncmp(relative_path, "./", 2) == 0) {
        sprintf(result, "%s/%s", normalized_base, relative_path + 2);
    } else if (strncmp(relative_path, "../", 3) == 0) {
        // Remonter puis descendre
        char* last_slash = strrchr(normalized_base, '/');
        if (last_slash) {
            *last_slash = '\0';
            sprintf(result, "%s/%s", normalized_base, relative_path + 3);
        } else {
            sprintf(result, "./%s", relative_path + 3);
        }
    } else {
        // Traiter comme ./path
        sprintf(result, "%s/%s", normalized_base, relative_path);
    }
    
    free(normalized_base);
    return result;
}

static char* find_module_path(const char* module_name, const char* from_package) {
    char* path = NULL;
    
    // 1. Vérifier si c'est un chemin relatif
    if (module_name[0] == '.' || module_name[0] == '/') {
        path = resolve_relative_path(current_dir, module_name);
        
        // Ajouter l'extension .swf si nécessaire
        if (path && access(path, F_OK) != 0) {
            char* with_ext = malloc(strlen(path) + 5);
            sprintf(with_ext, "%s.swf", path);
            if (access(with_ext, F_OK) == 0) {
                free(path);
                path = with_ext;
            } else {
                free(with_ext);
            }
        }
        
        if (path && access(path, F_OK) == 0) {
            printf(CYAN "[RESOLVE]" RESET " Found local module: %s\n", path);
            return path;
        }
        
        free(path);
    }
    
    // 2. Si from_package est spécifié, chercher dans le package
    if (from_package) {
        Package* pkg = find_package(from_package);
        if (pkg) {
            // Construire le chemin du package
            char package_path[512];
            sprintf(package_path, "/usr/local/lib/swift/%s", pkg->name);
            
            // Vérifier si c'est un module .swf
            char module_path[512];
            sprintf(module_path, "%s/%s.swf", package_path, module_name);
            if (access(module_path, F_OK) == 0) {
                path = str_copy(module_path);
                printf(CYAN "[RESOLVE]" RESET " Found module in package %s: %s\n", from_package, path);
                return path;
            }
            
            // Vérifier si c'est un sous-répertoire
            sprintf(module_path, "%s/%s/", package_path, module_name);
            struct stat st;
            if (stat(module_path, &st) == 0 && S_ISDIR(st.st_mode)) {
                // Chercher le manifest .svlib
                char manifest_path[512];
                sprintf(manifest_path, "%s/%s.svlib", module_path, module_name);
                if (access(manifest_path, F_OK) == 0) {
                    path = str_copy(manifest_path);
                    return path;
                }
            }
        }
    }
    
    // 3. Chercher dans /usr/local/lib/swift/
    char* base_path = "/usr/local/lib/swift/";
    
    // Essayer module.swf directement
    char system_path[512];
    sprintf(system_path, "%s%s.swf", base_path, module_name);
    if (access(system_path, F_OK) == 0) {
        path = str_copy(system_path);
        printf(CYAN "[RESOLVE]" RESET " Found system module: %s\n", path);
        return path;
    }
    
    // Essayer comme package (dossier)
    sprintf(system_path, "%s%s/", base_path, module_name);
    struct stat st;
    if (stat(system_path, &st) == 0 && S_ISDIR(st.st_mode)) {
        // Chercher le manifest .svlib
        char manifest_path[512];
        sprintf(manifest_path, "%s%s.svlib", system_path, module_name);
        if (access(manifest_path, F_OK) == 0) {
            path = str_copy(manifest_path);
            printf(CYAN "[RESOLVE]" RESET " Found package manifest: %s\n", path);
            return path;
        }
        
        // Chercher module.swf dans le package
        sprintf(manifest_path, "%s%s.swf", system_path, module_name);
        if (access(manifest_path, F_OK) == 0) {
            path = str_copy(manifest_path);
            return path;
        }
    }
    
    // 4. Si current_package est défini, chercher dedans
    if (current_package) {
        char package_path[512];
        sprintf(package_path, "/usr/local/lib/swift/%s/%s.swf", current_package, module_name);
        if (access(package_path, F_OK) == 0) {
            path = str_copy(package_path);
            return path;
        }
    }
    
    printf(RED "[RESOLVE]" RESET " Module not found: %s\n", module_name);
    return NULL;
}

// ======================================================
// [SECTION] PACKAGE MANAGEMENT
// ======================================================
Package* find_package(const char* name) {
    Package* pkg = package_list;
    while (pkg) {
        if (strcmp(pkg->name, name) == 0) {
            return pkg;
        }
        pkg = pkg->next;
    }
    return NULL;
}

Package* create_package(const char* name, const char* path) {
    Package* pkg = malloc(sizeof(Package));
    if (pkg) {
        strncpy(pkg->name, name, sizeof(pkg->name) - 1);
        pkg->name[sizeof(pkg->name) - 1] = '\0';
        pkg->path = str_copy(path);
        pkg->exports = NULL;
        pkg->export_count = 0;
        pkg->classes = NULL;
        pkg->class_count = 0;
        pkg->loaded = false;
        pkg->next = package_list;
        package_list = pkg;
    }
    return pkg;
}

void add_package_export(Package* pkg, Variable* var) {
    if (!pkg || !var) return;
    
    pkg->export_count++;
    pkg->exports = realloc(pkg->exports, pkg->export_count * sizeof(Variable*));
    pkg->exports[pkg->export_count - 1] = var;
    var->is_exported = true;
    
    printf(CYAN "[PACKAGE]" RESET " %s exports variable: %s\n", pkg->name, var->name);
}

void add_package_class(Package* pkg, ClassDefinition* cls) {
    if (!pkg || !cls) return;
    
    pkg->class_count++;
    pkg->classes = realloc(pkg->classes, pkg->class_count * sizeof(ClassDefinition*));
    pkg->classes[pkg->class_count - 1] = cls;
    
    printf(CYAN "[PACKAGE]" RESET " %s exports class: %s\n", pkg->name, cls->name);
}

void load_package(const char* name, Error* error) {
    Package* pkg = find_package(name);
    if (pkg && pkg->loaded) {
        printf(CYAN "[PACKAGE]" RESET " Package already loaded: %s\n", name);
        return;
    }
    
    char* manifest_path = find_module_path(name, NULL);
    if (!manifest_path) {
        set_error(error, 0, 0, name, "Cannot find package: %s", name);
        return;
    }
    
    // Vérifier si c'est un fichier .svlib
    const char* ext = strrchr(manifest_path, '.');
    if (!ext || (strcmp(ext, ".svlib") != 0 && strcmp(ext, ".swf") != 0)) {
        free(manifest_path);
        set_error(error, 0, 0, name, "Invalid package file: %s", name);
        return;
    }
    
    // Lire le fichier
    FILE* f = fopen(manifest_path, "r");
    if (!f) {
        free(manifest_path);
        set_error(error, 0, 0, name, "Cannot open package file: %s", name);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        free(manifest_path);
        set_error(error, 0, 0, name, "Memory allocation failed");
        return;
    }
    
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    printf(CYAN "[PACKAGE]" RESET " Loading package: %s\n", name);
    
    // Sauvegarder l'état courant
    char* old_package = current_package ? str_copy(current_package) : NULL;
    char* old_module = current_module ? str_copy(current_module) : NULL;
    char* old_dir = current_dir ? str_copy(current_dir) : NULL;
    
    // Définir le package courant
    current_package = str_copy(name);
    current_module = str_copy(name);
    
    // Extraire le répertoire du manifest
    char* manifest_dir = strdup(manifest_path);
    char* last_slash = strrchr(manifest_dir, '/');
    if (last_slash) {
        *last_slash = '\0';
        current_dir = str_copy(manifest_dir);
    }
    free(manifest_dir);
    
    // Créer ou récupérer le package
    if (!pkg) {
        pkg = create_package(name, manifest_path);
    }
    
    // Parser et exécuter le manifest
    int node_count = 0;
    ASTNode** nodes = parse(source, &node_count, error);
    
    if (nodes && !error->message[0]) {
        // Exécuter les déclarations
        for (int i = 0; i < node_count; i++) {
            if (nodes[i]) {
                execute(nodes[i], error);
                if (error->message[0]) break;
                
                // Nettoyer le nœud
                free_ast_node(nodes[i]);
            }
        }
        free(nodes);
    }
    
    // Marquer comme chargé
    if (pkg && !error->message[0]) {
        pkg->loaded = true;
    }
    
    // Restaurer l'état
    free(current_package);
    free(current_module);
    free(current_dir);
    current_package = old_package;
    current_module = old_module;
    current_dir = old_dir;
    
    free(source);
    free(manifest_path);
}

// ======================================================
// [SECTION] IMPORT EXECUTION
// ======================================================
static void execute_import(ASTNode* node, Error* error) {
    if (!node || !node->data.import_export.modules) return;
    
    for (int i = 0; i < node->data.import_export.module_count; i++) {
        char* module_name = node->data.import_export.modules[i];
        char* from_package = node->data.import_export.from_module;
        
        printf(CYAN "[IMPORT]" RESET " Importing: %s", module_name);
        if (from_package) printf(" from package: %s", from_package);
        printf("\n");
        
        // Wildcard import
        if (strcmp(module_name, "*") == 0 && from_package) {
            load_package(from_package, error);
            continue;
        }
        
        // Charger le package si nécessaire
        if (from_package) {
            load_package(from_package, error);
            if (error->message[0]) return;
        }
        
        // Trouver le chemin du module
        char* module_path = find_module_path(module_name, from_package);
        if (!module_path) {
            set_error(error, node->line, node->column, current_module, 
                     "Cannot find module: %s", module_name);
            return;
        }
        
        // Vérifier si c'est un package manifest
        const char* ext = strrchr(module_path, '.');
        if (ext && strcmp(ext, ".svlib") == 0) {
            // Extraire le nom du package du chemin
            char* package_name = strdup(module_name);
            char* dot = strrchr(package_name, '.');
            if (dot) *dot = '\0';
            
            load_package(package_name, error);
            free(package_name);
            free(module_path);
            continue;
        }
        
        // Ouvrir et exécuter le module
        FILE* f = fopen(module_path, "r");
        if (!f) {
            set_error(error, node->line, node->column, current_module,
                     "Cannot open module: %s", module_path);
            free(module_path);
            return;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        if (!source) {
            fclose(f);
            free(module_path);
            set_error(error, node->line, node->column, current_module,
                     "Memory allocation failed");
            return;
        }
        
        fread(source, 1, size, f);
        source[size] = '\0';
        fclose(f);
        
        // Sauvegarder l'état
        char* old_module = current_module ? str_copy(current_module) : NULL;
        char* old_dir = current_dir ? str_copy(current_dir) : NULL;
        
        // Définir le module courant
        current_module = str_copy(module_name);
        
        // Extraire le répertoire du module
        char* module_dir = strdup(module_path);
        char* last_slash = strrchr(module_dir, '/');
        if (last_slash) {
            *last_slash = '\0';
            current_dir = str_copy(module_dir);
        } else {
            current_dir = str_copy(".");
        }
        free(module_dir);
        
        // Parser et exécuter le module
        int node_count = 0;
        ASTNode** nodes = parse(source, &node_count, error);
        
        if (nodes && !error->message[0]) {
            // Exécuter d'abord les imports
            for (int j = 0; j < node_count; j++) {
                if (nodes[j] && nodes[j]->type == NODE_IMPORT) {
                    execute_import(nodes[j], error);
                    if (error->message[0]) break;
                }
            }
            
            // Exécuter le reste (sauf les imports déjà faits)
            for (int j = 0; j < node_count && !error->message[0]; j++) {
                if (nodes[j] && nodes[j]->type != NODE_IMPORT) {
                    execute(nodes[j], error);
                }
            }
            
            // Nettoyer
            for (int j = 0; j < node_count; j++) {
                if (nodes[j]) {
                    free_ast_node(nodes[j]);
                }
            }
            free(nodes);
        }
        
        // Restaurer l'état
        free(current_module);
        free(current_dir);
        current_module = old_module;
        current_dir = old_dir;
        
        free(source);
        free(module_path);
        
        if (error->message[0]) return;
    }
}

// ======================================================
// [SECTION] VALUE CREATION & MANAGEMENT
// ======================================================
Value* create_value(ValueType type) {
    Value* value = malloc(sizeof(Value));
    if (value) {
        value->type = type;
        value->is_constant = false;
        value->name = NULL;
        value->ref_count = 1;
        
        // Initialiser selon le type
        switch (type) {
            case TYPE_INT:
                value->as.int_val = 0;
                break;
            case TYPE_FLOAT:
                value->as.float_val = 0.0;
                break;
            case TYPE_STRING:
                value->as.str_val = NULL;
                break;
            case TYPE_BOOL:
                value->as.bool_val = false;
                break;
            case TYPE_CHAR:
                value->as.char_val = '\0';
                break;
            case TYPE_ARRAY:
                value->as.array.elements = NULL;
                value->as.array.length = 0;
                value->as.array.capacity = 0;
                break;
            case TYPE_MAP:
                value->as.map.keys = NULL;
                value->as.map.values = NULL;
                value->as.map.count = 0;
                value->as.map.capacity = 0;
                break;
            case TYPE_OBJECT:
                value->as.object.class_name = NULL;
                value->as.object.fields = NULL;
                value->as.object.field_count = 0;
                break;
            case TYPE_FUNCTION:
                value->as.func.func_node = NULL;
                value->as.func.closure = NULL;
                break;
            case TYPE_PROMISE:
                value->as.promise.thread = 0;
                value->as.promise.result = NULL;
                value->as.promise.resolved = false;
                value->as.promise.rejected = false;
                value->as.promise.error = NULL;
                break;
            default:
                value->as.ptr_val = NULL;
                break;
        }
    }
    return value;
}

void free_value(Value* value) {
    if (!value) return;
    
    value->ref_count--;
    if (value->ref_count > 0) return;
    
    switch (value->type) {
        case TYPE_STRING:
            if (value->as.str_val) free(value->as.str_val);
            break;
        case TYPE_ARRAY:
            if (value->as.array.elements) {
                for (int i = 0; i < value->as.array.length; i++) {
                    if (value->as.array.elements[i]) {
                        free_value(value->as.array.elements[i]);
                    }
                }
                free(value->as.array.elements);
            }
            break;
        case TYPE_MAP:
            if (value->as.map.keys && value->as.map.values) {
                for (int i = 0; i < value->as.map.count; i++) {
                    if (value->as.map.keys[i]) free(value->as.map.keys[i]);
                    if (value->as.map.values[i]) free_value(value->as.map.values[i]);
                }
                free(value->as.map.keys);
                free(value->as.map.values);
            }
            break;
        case TYPE_OBJECT:
            if (value->as.object.class_name) free(value->as.object.class_name);
            if (value->as.object.fields) {
                for (int i = 0; i < value->as.object.field_count; i++) {
                    if (value->as.object.fields[i]) {
                        free_value(value->as.object.fields[i]);
                    }
                }
                free(value->as.object.fields);
            }
            break;
        case TYPE_FUNCTION:
            if (value->as.func.closure) free_value(value->as.func.closure);
            break;
        case TYPE_PROMISE:
            if (value->as.promise.result) free_value(value->as.promise.result);
            if (value->as.promise.error) free_value(value->as.promise.error);
            break;
        default:
            break;
    }
    
    if (value->name) free(value->name);
    free(value);
}

Value* copy_value(const Value* src) {
    if (!src) return NULL;
    
    Value* dest = create_value(src->type);
    if (!dest) return NULL;
    
    dest->is_constant = src->is_constant;
    
    switch (src->type) {
        case TYPE_INT:
            dest->as.int_val = src->as.int_val;
            break;
        case TYPE_FLOAT:
            dest->as.float_val = src->as.float_val;
            break;
        case TYPE_STRING:
            if (src->as.str_val) {
                dest->as.str_val = str_copy(src->as.str_val);
            }
            break;
        case TYPE_BOOL:
            dest->as.bool_val = src->as.bool_val;
            break;
        case TYPE_CHAR:
            dest->as.char_val = src->as.char_val;
            break;
        case TYPE_ARRAY:
            dest->as.array.length = src->as.array.length;
            dest->as.array.capacity = src->as.array.capacity;
            if (src->as.array.elements) {
                dest->as.array.elements = malloc(src->as.array.capacity * sizeof(Value*));
                for (int i = 0; i < src->as.array.length; i++) {
                    dest->as.array.elements[i] = copy_value(src->as.array.elements[i]);
                }
            }
            break;
        case TYPE_MAP:
            dest->as.map.count = src->as.map.count;
            dest->as.map.capacity = src->as.map.capacity;
            if (src->as.map.keys && src->as.map.values) {
                dest->as.map.keys = malloc(src->as.map.capacity * sizeof(char*));
                dest->as.map.values = malloc(src->as.map.capacity * sizeof(Value*));
                for (int i = 0; i < src->as.map.count; i++) {
                    dest->as.map.keys[i] = str_copy(src->as.map.keys[i]);
                    dest->as.map.values[i] = copy_value(src->as.map.values[i]);
                }
            }
            break;
        case TYPE_OBJECT:
            if (src->as.object.class_name) {
                dest->as.object.class_name = str_copy(src->as.object.class_name);
            }
            dest->as.object.field_count = src->as.object.field_count;
            if (src->as.object.fields) {
                dest->as.object.fields = malloc(src->as.object.field_count * sizeof(Value*));
                for (int i = 0; i < src->as.object.field_count; i++) {
                    dest->as.object.fields[i] = copy_value(src->as.object.fields[i]);
                }
            }
            break;
        case TYPE_FUNCTION:
            dest->as.func.func_node = src->as.func.func_node;
            dest->as.func.closure = copy_value(src->as.func.closure);
            break;
        case TYPE_PROMISE:
            dest->as.promise.result = copy_value(src->as.promise.result);
            dest->as.promise.error = copy_value(src->as.promise.error);
            dest->as.promise.resolved = src->as.promise.resolved;
            dest->as.promise.rejected = src->as.promise.rejected;
            break;
        default:
            break;
    }
    
    return dest;
}

char* value_to_string(const Value* value) {
    if (!value) return str_copy("null");
    
    char buffer[1024];
    
    switch (value->type) {
        case TYPE_INT:
            snprintf(buffer, sizeof(buffer), "%lld", value->as.int_val);
            break;
        case TYPE_FLOAT:
            // Afficher comme entier si possible
            if (value->as.float_val == (int64_t)value->as.float_val) {
                snprintf(buffer, sizeof(buffer), "%lld", (int64_t)value->as.float_val);
            } else {
                // Supprimer les zéros inutiles
                snprintf(buffer, sizeof(buffer), "%.10f", value->as.float_val);
                char* dot = strchr(buffer, '.');
                if (dot) {
                    char* end = buffer + strlen(buffer) - 1;
                    while (end > dot && *end == '0') *end-- = '\0';
                    if (end == dot) *dot = '\0';
                }
            }
            break;
        case TYPE_STRING:
            if (value->as.str_val) {
                return str_format("\"%s\"", value->as.str_val);
            }
            strcpy(buffer, "\"\"");
            break;
        case TYPE_BOOL:
            strcpy(buffer, value->as.bool_val ? "true" : "false");
            break;
        case TYPE_CHAR:
            snprintf(buffer, sizeof(buffer), "'%c'", value->as.char_val);
            break;
        case TYPE_ARRAY:
            strcpy(buffer, "[");
            for (int i = 0; i < value->as.array.length; i++) {
                char* elem_str = value_to_string(value->as.array.elements[i]);
                strcat(buffer, elem_str);
                free(elem_str);
                if (i < value->as.array.length - 1) strcat(buffer, ", ");
            }
            strcat(buffer, "]");
            break;
        case TYPE_MAP:
            strcpy(buffer, "{");
            for (int i = 0; i < value->as.map.count; i++) {
                strcat(buffer, value->as.map.keys[i]);
                strcat(buffer, ": ");
                char* val_str = value_to_string(value->as.map.values[i]);
                strcat(buffer, val_str);
                free(val_str);
                if (i < value->as.map.count - 1) strcat(buffer, ", ");
            }
            strcat(buffer, "}");
            break;
        case TYPE_NULL:
            strcpy(buffer, "null");
            break;
        case TYPE_UNDEFINED:
            strcpy(buffer, "undefined");
            break;
        default:
            snprintf(buffer, sizeof(buffer), "<%s object>", 
                    value->type == TYPE_OBJECT && value->as.object.class_name ? 
                    value->as.object.class_name : "unknown");
            break;
    }
    
    return str_copy(buffer);
}

bool values_equal(const Value* a, const Value* b) {
    if (!a || !b) return a == b;
    if (a->type != b->type) return false;
    
    switch (a->type) {
        case TYPE_INT:
            return a->as.int_val == b->as.int_val;
        case TYPE_FLOAT:
            return fabs(a->as.float_val - b->as.float_val) < 1e-10;
        case TYPE_STRING:
            if (!a->as.str_val || !b->as.str_val) return a->as.str_val == b->as.str_val;
            return strcmp(a->as.str_val, b->as.str_val) == 0;
        case TYPE_BOOL:
            return a->as.bool_val == b->as.bool_val;
        case TYPE_CHAR:
            return a->as.char_val == b->as.char_val;
        case TYPE_NULL:
        case TYPE_UNDEFINED:
            return true;
        default:
            return false;
    }
}

// ======================================================
// [SECTION] EXECUTION FUNCTIONS
// ======================================================
static Value* evaluate(ASTNode* node, Error* error) {
    if (!node) return create_value(TYPE_NULL);
    
    Value* result = NULL;
    
    switch (node->type) {
        case NODE_INT:
            result = create_value(TYPE_INT);
            result->as.int_val = node->data.int_val;
            break;
        case NODE_FLOAT:
            result = create_value(TYPE_FLOAT);
            result->as.float_val = node->data.float_val;
            break;
        case NODE_STRING:
            result = create_value(TYPE_STRING);
            result->as.str_val = str_copy(node->data.str_val);
            break;
        case NODE_CHAR:
            result = create_value(TYPE_CHAR);
            result->as.char_val = node->data.char_val;
            break;
        case NODE_BOOL:
            result = create_value(TYPE_BOOL);
            result->as.bool_val = node->data.bool_val;
            break;
        case NODE_NULL:
            result = create_value(TYPE_NULL);
            break;
        case NODE_UNDEFINED:
            result = create_value(TYPE_UNDEFINED);
            break;
        case NODE_IDENT:
            {
                Variable* var = find_variable(node->data.name);
                if (var && var->value) {
                    result = copy_value(var->value);
                } else {
                    set_error(error, node->line, node->column, current_module,
                             "Undefined variable: %s", node->data.name);
                    result = create_value(TYPE_UNDEFINED);
                }
            }
            break;
        case NODE_BINARY:
            result = eval_binary(node, error);
            break;
        case NODE_UNARY:
            result = eval_unary(node, error);
            break;
        case NODE_ARRAY:
            result = eval_array(node, error);
            break;
        case NODE_MAP:
            result = eval_map(node, error);
            break;
        case NODE_CALL:
            result = eval_call(node, error);
            break;
        case NODE_MEMBER_ACCESS:
            result = eval_member_access(node, error);
            break;
        case NODE_NEW:
            result = eval_new(node, error);
            break;
        case NODE_THIS:
            // TODO: Implement this
            result = create_value(TYPE_OBJECT);
            break;
        case NODE_SUPER:
            // TODO: Implement super
            result = create_value(TYPE_OBJECT);
            break;
        case NODE_AWAIT:
            // TODO: Implement await
            result = create_value(TYPE_NULL);
            break;
        default:
            set_error(error, node->line, node->column, current_module,
                     "Cannot evaluate node type: %d", node->type);
            result = create_value(TYPE_NULL);
            break;
    }
    
    return result;
}

static Value* eval_binary(ASTNode* node, Error* error) {
    Value* left_val = evaluate(node->left, error);
    if (error->message[0]) return left_val;
    
    Value* right_val = evaluate(node->right, error);
    if (error->message[0]) {
        free_value(left_val);
        return right_val;
    }
    
    Value* result = NULL;
    
    switch (node->op_type) {
        case TK_PLUS:
            // Addition ou concaténation
            if (left_val->type == TYPE_STRING || right_val->type == TYPE_STRING) {
                char* left_str = value_to_string(left_val);
                char* right_str = value_to_string(right_val);
                result = create_value(TYPE_STRING);
                result->as.str_val = malloc(strlen(left_str) + strlen(right_str) + 1);
                strcpy(result->as.str_val, left_str);
                strcat(result->as.str_val, right_str);
                free(left_str);
                free(right_str);
            } else if (left_val->type == TYPE_FLOAT || right_val->type == TYPE_FLOAT) {
                result = create_value(TYPE_FLOAT);
                double left = (left_val->type == TYPE_FLOAT) ? left_val->as.float_val : (double)left_val->as.int_val;
                double right = (right_val->type == TYPE_FLOAT) ? right_val->as.float_val : (double)right_val->as.int_val;
                result->as.float_val = left + right;
            } else {
                result = create_value(TYPE_INT);
                result->as.int_val = left_val->as.int_val + right_val->as.int_val;
            }
            break;
            
        case TK_MINUS:
            if (left_val->type == TYPE_FLOAT || right_val->type == TYPE_FLOAT) {
                result = create_value(TYPE_FLOAT);
                double left = (left_val->type == TYPE_FLOAT) ? left_val->as.float_val : (double)left_val->as.int_val;
                double right = (right_val->type == TYPE_FLOAT) ? right_val->as.float_val : (double)right_val->as.int_val;
                result->as.float_val = left - right;
            } else {
                result = create_value(TYPE_INT);
                result->as.int_val = left_val->as.int_val - right_val->as.int_val;
            }
            break;
            
        case TK_MULT:
            if (left_val->type == TYPE_FLOAT || right_val->type == TYPE_FLOAT) {
                result = create_value(TYPE_FLOAT);
                double left = (left_val->type == TYPE_FLOAT) ? left_val->as.float_val : (double)left_val->as.int_val;
                double right = (right_val->type == TYPE_FLOAT) ? right_val->as.float_val : (double)right_val->as.int_val;
                result->as.float_val = left * right;
            } else {
                result = create_value(TYPE_INT);
                result->as.int_val = left_val->as.int_val * right_val->as.int_val;
            }
            break;
            
        case TK_DIV:
            if (right_val->type == TYPE_INT && right_val->as.int_val == 0) {
                set_error(error, node->line, node->column, current_module, "Division by zero");
                result = create_value(TYPE_NULL);
            } else if (left_val->type == TYPE_FLOAT || right_val->type == TYPE_FLOAT) {
                result = create_value(TYPE_FLOAT);
                double left = (left_val->type == TYPE_FLOAT) ? left_val->as.float_val : (double)left_val->as.int_val;
                double right = (right_val->type == TYPE_FLOAT) ? right_val->as.float_val : (double)right_val->as.int_val;
                result->as.float_val = left / right;
            } else {
                result = create_value(TYPE_INT);
                result->as.int_val = left_val->as.int_val / right_val->as.int_val;
            }
            break;
            
        case TK_MOD:
            if (right_val->type == TYPE_INT && right_val->as.int_val == 0) {
                set_error(error, node->line, node->column, current_module, "Modulo by zero");
                result = create_value(TYPE_NULL);
            } else {
                result = create_value(TYPE_INT);
                result->as.int_val = left_val->as.int_val % right_val->as.int_val;
            }
            break;
            
        case TK_EQ:
            result = create_value(TYPE_BOOL);
            result->as.bool_val = values_equal(left_val, right_val);
            break;
            
        case TK_NEQ:
            result = create_value(TYPE_BOOL);
            result->as.bool_val = !values_equal(left_val, right_val);
            break;
            
        case TK_GT:
            if (left_val->type == TYPE_FLOAT || right_val->type == TYPE_FLOAT) {
                double left = (left_val->type == TYPE_FLOAT) ? left_val->as.float_val : (double)left_val->as.int_val;
                double right = (right_val->type == TYPE_FLOAT) ? right_val->as.float_val : (double)right_val->as.int_val;
                result = create_value(TYPE_BOOL);
                result->as.bool_val = left > right;
            } else {
                result = create_value(TYPE_BOOL);
                result->as.bool_val = left_val->as.int_val > right_val->as.int_val;
            }
            break;
            
        case TK_LT:
            if (left_val->type == TYPE_FLOAT || right_val->type == TYPE_FLOAT) {
                double left = (left_val->type == TYPE_FLOAT) ? left_val->as.float_val : (double)left_val->as.int_val;
                double right = (right_val->type == TYPE_FLOAT) ? right_val->as.float_val : (double)right_val->as.int_val;
                result = create_value(TYPE_BOOL);
                result->as.bool_val = left < right;
            } else {
                result = create_value(TYPE_BOOL);
                result->as.bool_val = left_val->as.int_val < right_val->as.int_val;
            }
            break;
            
        case TK_GTE:
            if (left_val->type == TYPE_FLOAT || right_val->type == TYPE_FLOAT) {
                double left = (left_val->type == TYPE_FLOAT) ? left_val->as.float_val : (double)left_val->as.int_val;
                double right = (right_val->type == TYPE_FLOAT) ? right_val->as.float_val : (double)right_val->as.int_val;
                result = create_value(TYPE_BOOL);
                result->as.bool_val = left >= right;
            } else {
                result = create_value(TYPE_BOOL);
                result->as.bool_val = left_val->as.int_val >= right_val->as.int_val;
            }
            break;
            
        case TK_LTE:
            if (left_val->type == TYPE_FLOAT || right_val->type == TYPE_FLOAT) {
                double left = (left_val->type == TYPE_FLOAT) ? left_val->as.float_val : (double)left_val->as.int_val;
                double right = (right_val->type == TYPE_FLOAT) ? right_val->as.float_val : (double)right_val->as.int_val;
                result = create_value(TYPE_BOOL);
                result->as.bool_val = left <= right;
            } else {
                result = create_value(TYPE_BOOL);
                result->as.bool_val = left_val->as.int_val <= right_val->as.int_val;
            }
            break;
            
        case TK_AND:
            result = create_value(TYPE_BOOL);
            result->as.bool_val = (left_val->type == TYPE_BOOL ? left_val->as.bool_val : left_val->as.int_val != 0) &&
                                 (right_val->type == TYPE_BOOL ? right_val->as.bool_val : right_val->as.int_val != 0);
            break;
            
        case TK_OR:
            result = create_value(TYPE_BOOL);
            result->as.bool_val = (left_val->type == TYPE_BOOL ? left_val->as.bool_val : left_val->as.int_val != 0) ||
                                 (right_val->type == TYPE_BOOL ? right_val->as.bool_val : right_val->as.int_val != 0);
            break;
            
        default:
            set_error(error, node->line, node->column, current_module,
                     "Unsupported binary operator: %d", node->op_type);
            result = create_value(TYPE_NULL);
            break;
    }
    
    free_value(left_val);
    free_value(right_val);
    return result;
}

static Value* eval_unary(ASTNode* node, Error* error) {
    Value* operand = evaluate(node->left, error);
    if (error->message[0]) return operand;
    
    Value* result = NULL;
    
    switch (node->op_type) {
        case TK_MINUS:
            if (operand->type == TYPE_FLOAT) {
                result = create_value(TYPE_FLOAT);
                result->as.float_val = -operand->as.float_val;
            } else if (operand->type == TYPE_INT) {
                result = create_value(TYPE_INT);
                result->as.int_val = -operand->as.int_val;
            } else {
                set_error(error, node->line, node->column, current_module,
                         "Cannot apply '-' to non-numeric type");
                result = create_value(TYPE_NULL);
            }
            break;
            
        case TK_NOT:
            result = create_value(TYPE_BOOL);
            if (operand->type == TYPE_BOOL) {
                result->as.bool_val = !operand->as.bool_val;
            } else if (operand->type == TYPE_INT) {
                result->as.bool_val = operand->as.int_val == 0;
            } else if (operand->type == TYPE_FLOAT) {
                result->as.bool_val = operand->as.float_val == 0.0;
            } else if (operand->type == TYPE_NULL || operand->type == TYPE_UNDEFINED) {
                result->as.bool_val = true;
            } else if (operand->type == TYPE_STRING) {
                result->as.bool_val = (operand->as.str_val == NULL || strlen(operand->as.str_val) == 0);
            } else {
                result->as.bool_val = false;
            }
            break;
            
        case TK_INC:
        case TK_DEC:
            // TODO: Implement increment/decrement
            result = copy_value(operand);
            break;
            
        case TK_BIT_NOT:
            if (operand->type == TYPE_INT) {
                result = create_value(TYPE_INT);
                result->as.int_val = ~operand->as.int_val;
            } else {
                set_error(error, node->line, node->column, current_module,
                         "Cannot apply '~' to non-integer type");
                result = create_value(TYPE_NULL);
            }
            break;
            
        default:
            set_error(error, node->line, node->column, current_module,
                     "Unsupported unary operator: %d", node->op_type);
            result = create_value(TYPE_NULL);
            break;
    }
    
    free_value(operand);
    return result;
}

static Value* eval_array(ASTNode* node, Error* error) {
    Value* result = create_value(TYPE_ARRAY);
    
    if (node->data.array.element_count > 0) {
        result->as.array.length = node->data.array.element_count;
        result->as.array.capacity = node->data.array.element_count;
        result->as.array.elements = malloc(node->data.array.element_count * sizeof(Value*));
        
        for (int i = 0; i < node->data.array.element_count; i++) {
            result->as.array.elements[i] = evaluate(node->data.array.elements[i], error);
            if (error->message[0]) {
                // Cleanup on error
                for (int j = 0; j < i; j++) {
                    free_value(result->as.array.elements[j]);
                }
                free(result->as.array.elements);
                free_value(result);
                return NULL;
            }
        }
    }
    
    return result;
}

static Value* eval_map(ASTNode* node, Error* error) {
    Value* result = create_value(TYPE_MAP);
    
    if (node->data.map.pair_count > 0) {
        result->as.map.count = node->data.map.pair_count;
        result->as.map.capacity = node->data.map.pair_count;
        result->as.map.keys = malloc(node->data.map.pair_count * sizeof(char*));
        result->as.map.values = malloc(node->data.map.pair_count * sizeof(Value*));
        
        for (int i = 0; i < node->data.map.pair_count; i++) {
            result->as.map.keys[i] = str_copy(node->data.map.keys[i]);
            result->as.map.values[i] = evaluate(node->data.map.values[i], error);
            
            if (error->message[0]) {
                // Cleanup on error
                for (int j = 0; j <= i; j++) {
                    if (j < i) free(result->as.map.keys[j]);
                    if (result->as.map.values[j]) free_value(result->as.map.values[j]);
                }
                free(result->as.map.keys);
                free(result->as.map.values);
                free_value(result);
                return NULL;
            }
        }
    }
    
    return result;
}

static Value* eval_call(ASTNode* node, Error* error) {
    // TODO: Implement function calls
    // For now, just evaluate arguments and return null
    if (node->right && node->right->type == NODE_ARRAY) {
        for (int i = 0; i < node->right->data.array.element_count; i++) {
            Value* arg = evaluate(node->right->data.array.elements[i], error);
            if (error->message[0]) {
                free_value(arg);
                return create_value(TYPE_NULL);
            }
            free_value(arg);
        }
    }
    
    return create_value(TYPE_NULL);
}

static Value* eval_member_access(ASTNode* node, Error* error) {
    // TODO: Implement member access
    return create_value(TYPE_NULL);
}

static Value* eval_new(ASTNode* node, Error* error) {
    // TODO: Implement object creation
    return create_value(TYPE_OBJECT);
}

// ======================================================
// [SECTION] EXECUTE STATEMENTS
// ======================================================
void execute(ASTNode* node, Error* error) {
    if (!node || error->message[0]) return;
    
    switch (node->type) {
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL:
            {
                // Determine variable type
                ValueType var_type = TYPE_INT;
                switch (node->type) {
                    case NODE_NET_DECL: var_type = TYPE_INT; break;  // Network values are ints
                    case NODE_CLOG_DECL: var_type = TYPE_BOOL; break; // Complex logic = bool
                    case NODE_DOS_DECL: var_type = TYPE_STRING; break; // Large data = string
                    case NODE_SEL_DECL: var_type = TYPE_INT; break;   // Selection = int
                    case NODE_CONST_DECL: var_type = TYPE_INT; break;
                    default: var_type = TYPE_INT; break;
                }
                
                Variable* var = create_variable(node->data.name, var_type);
                if (!var) {
                    set_error(error, node->line, node->column, current_module,
                             "Failed to create variable: %s", node->data.name);
                    return;
                }
                
                var->is_constant = (node->type == NODE_CONST_DECL);
                
                // Initialize if needed
                if (node->left) {
                    Value* init_val = evaluate(node->left, error);
                    if (!error->message[0] && init_val) {
                        free_value(var->value);
                        var->value = init_val;
                        
                        // Add to dbvar table
                        char* value_str = value_to_string(init_val);
                        add_to_dbvar(var, value_str);
                        free(value_str);
                        
                        // Print declaration
                        const char* type_name = "";
                        switch (node->type) {
                            case NODE_VAR_DECL: type_name = "var"; break;
                            case NODE_NET_DECL: type_name = "net"; break;
                            case NODE_CLOG_DECL: type_name = "clog"; break;
                            case NODE_DOS_DECL: type_name = "dos"; break;
                            case NODE_SEL_DECL: type_name = "sel"; break;
                            case NODE_CONST_DECL: type_name = "const"; break;
                        }
                        
                        char* val_str = value_to_string(init_val);
                        printf(GREEN "[DECL]" RESET " %s %s = %s\n", type_name, node->data.name, val_str);
                        free(val_str);
                    }
                } else {
                    // Uninitialized
                    add_to_dbvar(var, "uninitialized");
                    printf(GREEN "[DECL]" RESET " %s %s (uninitialized)\n", 
                           node->type == NODE_CONST_DECL ? "const" : "var", node->data.name);
                }
            }
            break;
            
        case NODE_ASSIGN:
            {
                Variable* var = find_variable(node->data.name);
                if (!var) {
                    set_error(error, node->line, node->column, current_module,
                             "Undefined variable: %s", node->data.name);
                    return;
                }
                
                if (var->is_constant) {
                    set_error(error, node->line, node->column, current_module,
                             "Cannot assign to constant: %s", node->data.name);
                    return;
                }
                
                if (node->left) {
                    Value* new_val = evaluate(node->left, error);
                    if (!error->message[0] && new_val) {
                        free_value(var->value);
                        var->value = new_val;
                        
                        // Update dbvar
                        char* value_str = value_to_string(new_val);
                        update_dbvar(var, value_str);
                        free(value_str);
                        
                        // Print assignment
                        char* val_str = value_to_string(new_val);
                        printf(CYAN "[ASSIGN]" RESET " %s = %s\n", node->data.name, val_str);
                        free(val_str);
                    }
                }
            }
            break;
            
        case NODE_PRINT:
            if (node->left) {
                Value* val = evaluate(node->left, error);
                if (!error->message[0] && val) {
                    char* str = value_to_string(val);
                    printf("%s\n", str);
                    free(str);
                    free_value(val);
                }
            } else {
                printf("\n");
            }
            break;
            
        case NODE_IF:
            {
                Value* cond = evaluate(node->left, error);
                if (error->message[0]) {
                    free_value(cond);
                    return;
                }
                
                bool condition = false;
                if (cond->type == TYPE_BOOL) {
                    condition = cond->as.bool_val;
                } else if (cond->type == TYPE_INT) {
                    condition = cond->as.int_val != 0;
                } else if (cond->type == TYPE_FLOAT) {
                    condition = cond->as.float_val != 0.0;
                } else if (cond->type == TYPE_STRING) {
                    condition = cond->as.str_val && strlen(cond->as.str_val) > 0;
                } else if (cond->type == TYPE_NULL || cond->type == TYPE_UNDEFINED) {
                    condition = false;
                } else {
                    condition = true;
                }
                
                free_value(cond);
                
                if (condition) {
                    execute(node->right, error);
                } else if (node->third) {
                    execute(node->third, error);
                }
            }
            break;
            
        case NODE_WHILE:
            while (1) {
                Value* cond = evaluate(node->left, error);
                if (error->message[0]) {
                    free_value(cond);
                    return;
                }
                
                bool condition = false;
                if (cond->type == TYPE_BOOL) {
                    condition = cond->as.bool_val;
                } else if (cond->type == TYPE_INT) {
                    condition = cond->as.int_val != 0;
                } else if (cond->type == TYPE_FLOAT) {
                    condition = cond->as.float_val != 0.0;
                }
                
                free_value(cond);
                
                if (!condition) break;
                
                execute(node->right, error);
                if (error->message[0]) return;
            }
            break;
            
        case NODE_FOR:
            // Execute initializer
            if (node->data.loop.init) {
                execute(node->data.loop.init, error);
                if (error->message[0]) return;
            }
            
            // Loop
            while (1) {
                // Check condition
                if (node->data.loop.condition) {
                    Value* cond = evaluate(node->data.loop.condition, error);
                    if (error->message[0]) {
                        free_value(cond);
                        return;
                    }
                    
                    bool condition = false;
                    if (cond->type == TYPE_BOOL) {
                        condition = cond->as.bool_val;
                    } else if (cond->type == TYPE_INT) {
                        condition = cond->as.int_val != 0;
                    } else if (cond->type == TYPE_FLOAT) {
                        condition = cond->as.float_val != 0.0;
                    }
                    
                    free_value(cond);
                    
                    if (!condition) break;
                }
                
                // Execute body
                execute(node->data.loop.body, error);
                if (error->message[0]) return;
                
                // Execute update
                if (node->data.loop.update) {
                    Value* update_val = evaluate(node->data.loop.update, error);
                    if (error->message[0]) {
                        free_value(update_val);
                        return;
                    }
                    free_value(update_val);
                }
            }
            break;
            
        case NODE_BLOCK:
            enter_scope();
            {
                ASTNode* current = node->left;
                while (current) {
                    execute(current, error);
                    if (error->message[0]) break;
                    current = current->right;
                }
            }
            exit_scope();
            break;
            
        case NODE_IMPORT:
            execute_import(node, error);
            break;
            
        case NODE_EXPORT:
            if (node->data.import_export.modules && node->data.import_export.module_count > 0) {
                // Export variable
                Variable* var = find_variable(node->data.import_export.modules[0]);
                if (var && current_package) {
                    Package* pkg = find_package(current_package);
                    if (pkg) {
                        add_package_export(pkg, var);
                        printf(CYAN "[EXPORT]" RESET " Exporting %s from package %s\n", 
                               var->name, current_package);
                    }
                } else {
                    printf(RED "[ERROR]" RESET " Cannot export undefined variable or no current package\n");
                }
            } else if (node->left) {
                // Export declaration (already executed)
                // Just mark as exported if it's a variable
                if (node->left->type >= NODE_VAR_DECL && node->left->type <= NODE_CONST_DECL) {
                    Variable* var = find_variable(node->left->data.name);
                    if (var && current_package) {
                        Package* pkg = find_package(current_package);
                        if (pkg) {
                            add_package_export(pkg, var);
                        }
                    }
                }
            }
            break;
            
        case NODE_RETURN:
            // TODO: Implement return
            break;
            
        case NODE_MAIN:
            printf(CYAN "\n[EXEC]" RESET " Starting main()...\n");
            for (int i = 0; i < 60; i++) printf("=");
            printf("\n");
            
            execute(node->left, error);
            
            printf("\n" CYAN "[EXEC]" RESET " main() finished\n");
            break;
            
        case NODE_DBVAR:
            {
                printf(CYAN "\n╔══════════════════════════════════════════════════════════════════════════════════════════════════════╗\n");
                printf("║                                     TABLE DES VARIABLES (dbvar)                                       ║\n");
                printf("╠══════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
                printf("║  ID   Type      Name            Size        Value               Init  Scope  Module                  ║\n");
                printf("╠══════════════════════════════════════════════════════════════════════════════════════════════════════╣\n");
                
                for (int i = 0; i < dbvar_count; i++) {
                    DBVarEntry* entry = &dbvars[i];
                    printf("║ %-4d %-9s %-15s %-11d %-19s %-5s %-6d %-20s ║\n",
                           entry->id,
                           entry->type,
                           entry->name,
                           entry->size_bytes,
                           entry->value_str,
                           entry->initialized ? "✓" : "✗",
                           entry->scope_level,
                           entry->module);
                }
                
                if (dbvar_count == 0) {
                    printf("║                                   No variables declared                                           ║\n");
                }
                
                printf("╚══════════════════════════════════════════════════════════════════════════════════════════════════════╝\n" RESET);
            }
            break;
            
        case NODE_SIZEOF:
            {
                Variable* var = find_variable(node->data.size_info.var_name);
                if (var) {
                    printf("%d bytes\n", var->value ? 
                           (var->value->type == TYPE_STRING && var->value->as.str_val ? 
                            strlen(var->value->as.str_val) + 1 : 8) : 0);
                } else {
                    printf(RED "[ERROR]" RESET " Variable not found: %s\n", node->data.size_info.var_name);
                }
            }
            break;
            
        case NODE_BINARY:
            {
                Value* result = evaluate(node, error);
                if (!error->message[0] && result) {
                    char* str = value_to_string(result);
                    printf("%s\n", str);
                    free(str);
                    free_value(result);
                }
            }
            break;
            
        default:
            // Silently ignore other node types for now
            break;
    }
}

// ======================================================
// [SECTION] CLEANUP FUNCTIONS
// ======================================================
static void cleanup_variables() {
    // Clean global scope
    if (global_scope) {
        current_scope = global_scope;
        while (current_scope->child) {
            current_scope = current_scope->child;
        }
        
        // Exit all scopes from bottom up
        while (current_scope) {
            exit_scope();
            current_scope = current_scope->parent;
        }
    }
    
    // Reset global scope
    global_scope = create_scope(0);
    current_scope = global_scope;
}

static void cleanup_dbvar() {
    dbvar_count = 0;
    next_var_id = 1;
}

static void cleanup_packages() {
    Package* pkg = package_list;
    while (pkg) {
        Package* next = pkg->next;
        
        if (pkg->path) free(pkg->path);
        if (pkg->exports) free(pkg->exports);
        if (pkg->classes) free(pkg->classes);
        
        free(pkg);
        pkg = next;
    }
    package_list = NULL;
}

static void cleanup_classes() {
    ClassDefinition* cls = class_list;
    while (cls) {
        ClassDefinition* next = cls->next;
        
        if (cls->parent) free(cls->parent);
        if (cls->fields) {
            for (int i = 0; i < cls->field_count; i++) {
                if (cls->fields[i]) {
                    free_value(cls->fields[i]->value);
                    free(cls->fields[i]->name);
                    free(cls->fields[i]->module);
                    free(cls->fields[i]);
                }
            }
            free(cls->fields);
        }
        if (cls->methods) {
            for (int i = 0; i < cls->method_count; i++) {
                if (cls->methods[i]) {
                    free_ast_node(cls->methods[i]);
                }
            }
            free(cls->methods);
        }
        
        free(cls);
        cls = next;
    }
    class_list = NULL;
}

// ======================================================
// [SECTION] FILE LOADING
// ======================================================
static char* load_file(const char* filename, Error* error) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        set_error(error, 0, 0, filename, "Cannot open file: %s", filename);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        set_error(error, 0, 0, filename, "Memory allocation failed");
        return NULL;
    }
    
    size_t bytes_read = fread(source, 1, size, f);
    source[bytes_read] = '\0';
    fclose(f);
    
    return source;
}

// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void run(const char* source, const char* filename, Error* error) {
    // Reset state
    cleanup_variables();
    cleanup_dbvar();
    
    // Set current module
    if (current_module) free(current_module);
    current_module = str_copy(filename);
    
    // Set current directory
    if (current_dir) free(current_dir);
    char* file_copy = strdup(filename);
    char* last_slash = strrchr(file_copy, '/');
    if (last_slash) {
        *last_slash = '\0';
        current_dir = str_copy(file_copy);
    } else {
        current_dir = str_copy(".");
    }
    free(file_copy);
    
    // Parse source
    int node_count = 0;
    ASTNode** nodes = parse(source, &node_count, error);
    
    if (!nodes || error->message[0]) {
        if (nodes) {
            for (int i = 0; i < node_count; i++) {
                if (nodes[i]) free_ast_node(nodes[i]);
            }
            free(nodes);
        }
        return;
    }
    
    // Execute imports first
    for (int i = 0; i < node_count; i++) {
        if (nodes[i] && nodes[i]->type == NODE_IMPORT) {
            execute_import(nodes[i], error);
            if (error->message[0]) break;
        }
    }
    
    // Find and execute main() if it exists
    ASTNode* main_node = NULL;
    for (int i = 0; i < node_count && !error->message[0]; i++) {
        if (nodes[i] && nodes[i]->type == NODE_MAIN) {
            main_node = nodes[i];
            break;
        }
    }
    
    if (main_node && !error->message[0]) {
        execute(main_node, error);
    } else if (!error->message[0]) {
        // Execute all declarations in order (except imports already done)
        for (int i = 0; i < node_count && !error->message[0]; i++) {
            if (nodes[i] && nodes[i]->type != NODE_IMPORT) {
                execute(nodes[i], error);
            }
        }
    }
    
    // Cleanup AST nodes
    for (int i = 0; i < node_count; i++) {
        if (nodes[i]) {
            free_ast_node(nodes[i]);
        }
    }
    free(nodes);
}

// ======================================================
// [SECTION] REPL MODE
// ======================================================
static void repl() {
    printf(GREEN "╔════════════════════════════════════════════════════════════════════════════════════════════╗\n");
    printf("║                     SwiftFlow v%s - REPL Mode                                    ║\n", SWIFTFLOW_VERSION);
    printf("║                     Complete Language with Packages & Classes                     ║\n");
    printf("╚════════════════════════════════════════════════════════════════════════════════════════════╝\n" RESET);
    printf("\n");
    printf("Available commands:\n");
    printf("  " CYAN "exit" RESET "      - Quit the REPL\n");
    printf("  " CYAN "dbvar" RESET "     - Show variable table\n");
    printf("  " CYAN "clear" RESET "     - Clear screen\n");
    printf("  " CYAN "reset" RESET "     - Reset all variables\n");
    printf("  " CYAN "packages" RESET "  - Show loaded packages\n");
    printf("  " CYAN "classes" RESET "   - Show defined classes\n");
    printf("\n");
    printf("Example statements:\n");
    printf("  " YELLOW "var x = 10;" RESET "\n");
    printf("  " YELLOW "net port = 8080;" RESET "\n");
    printf("  " YELLOW "clog debug = true;" RESET "\n");
    printf("  " YELLOW "dos data = \"large content\";" RESET "\n");
    printf("  " YELLOW "sel option = 2;" RESET "\n");
    printf("  " YELLOW "print(x + 5);" RESET "\n");
    printf("  " YELLOW "if (x > 5) print(\"big\");" RESET "\n");
    printf("  " YELLOW "import \"math\" from \"stdlib\";" RESET "\n");
    printf("  " YELLOW "export myVar as \"public\";" RESET "\n");
    printf("\n");
    
    in_repl_mode = true;
    char line[4096];
    
    while (1) {
        printf(GREEN "swift> " RESET);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        // Handle commands
        if (strcmp(line, "exit") == 0) break;
        if (strcmp(line, "clear") == 0) {
            system("clear");
            continue;
        }
        if (strcmp(line, "dbvar") == 0) {
            execute(new_node(NODE_DBVAR), NULL);
            continue;
        }
        if (strcmp(line, "reset") == 0) {
            cleanup_variables();
            cleanup_dbvar();
            printf(GREEN "[INFO]" RESET " All variables reset\n");
            continue;
        }
        if (strcmp(line, "packages") == 0) {
            printf(CYAN "\n╔════════════════════════════════════════════════════════════════════╗\n");
            printf("║                     LOADED PACKAGES                                 ║\n");
            printf("╠════════════════════════════════════════════════════════════════════╣\n");
            
            int count = 0;
            Package* pkg = package_list;
            while (pkg) {
                printf("║ %-30s │ Exports: %-3d │ Loaded: %-5s ║\n",
                       pkg->name, pkg->export_count, pkg->loaded ? "✓" : "✗");
                count++;
                pkg = pkg->next;
            }
            
            if (count == 0) {
                printf("║                   No packages loaded                            ║\n");
            }
            
            printf("╚════════════════════════════════════════════════════════════════════╝\n" RESET);
            continue;
        }
        if (strcmp(line, "classes") == 0) {
            printf(CYAN "\n╔════════════════════════════════════════════════════════════════════╗\n");
            printf("║                     DEFINED CLASSES                                 ║\n");
            printf("╠════════════════════════════════════════════════════════════════════╣\n");
            
            int count = 0;
            ClassDefinition* cls = class_list;
            while (cls) {
                printf("║ %-30s │ Fields: %-3d │ Methods: %-3d ║\n",
                       cls->name, cls->field_count, cls->method_count);
                count++;
                cls = cls->next;
            }
            
            if (count == 0) {
                printf("║                   No classes defined                            ║\n");
            }
            
            printf("╚════════════════════════════════════════════════════════════════════╝\n" RESET);
            continue;
        }
        if (strlen(line) == 0) continue;
        
        Error error = {0};
        run(line, "REPL", &error);
        
        if (error.message[0]) {
            print_error(&error);
            clear_error(&error);
        }
    }
    
    in_repl_mode = false;
    printf(CYAN "\n[REPL]" RESET " Exiting SwiftFlow. Goodbye!\n");
}

// ======================================================
// [SECTION] AST NODE CLEANUP
// ======================================================
void free_ast_node(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_STRING:
            if (node->data.str_val) free(node->data.str_val);
            break;
        case NODE_IDENT:
            if (node->data.name) free(node->data.name);
            break;
        case NODE_ARRAY:
            if (node->data.array.elements) {
                for (int i = 0; i < node->data.array.element_count; i++) {
                    if (node->data.array.elements[i]) {
                        free_ast_node(node->data.array.elements[i]);
                    }
                }
                free(node->data.array.elements);
            }
            break;
        case NODE_MAP:
            if (node->data.map.keys && node->data.map.values) {
                for (int i = 0; i < node->data.map.pair_count; i++) {
                    if (node->data.map.keys[i]) free(node->data.map.keys[i]);
                    if (node->data.map.values[i]) free_ast_node(node->data.map.values[i]);
                }
                free(node->data.map.keys);
                free(node->data.map.values);
            }
            break;
        case NODE_IMPORT:
            if (node->data.import_export.modules) {
                for (int i = 0; i < node->data.import_export.module_count; i++) {
                    if (node->data.import_export.modules[i]) {
                        free(node->data.import_export.modules[i]);
                    }
                }
                free(node->data.import_export.modules);
            }
            if (node->data.import_export.from_module) free(node->data.import_export.from_module);
            if (node->data.import_export.alias) free(node->data.import_export.alias);
            break;
        case NODE_EXPORT:
            if (node->data.import_export.modules) {
                for (int i = 0; i < node->data.import_export.module_count; i++) {
                    if (node->data.import_export.modules[i]) free(node->data.import_export.modules[i]);
                }
                free(node->data.import_export.modules);
            }
            if (node->data.import_export.alias) free(node->data.import_export.alias);
            break;
        case NODE_SIZEOF:
            if (node->data.size_info.var_name) free(node->data.size_info.var_name);
            break;
        case NODE_FUNC:
        case NODE_ASYNC_FUNC:
            if (node->data.func.params) {
                for (int i = 0; i < node->data.func.param_count; i++) {
                    if (node->data.func.params[i]) free(node->data.func.params[i]);
                }
                free(node->data.func.params);
            }
            if (node->data.func.body) free_ast_node(node->data.func.body);
            break;
        case NODE_CLASS:
            if (node->data.class_def.class_name) free(node->data.class_def.class_name);
            if (node->data.class_def.parent_class) free(node->data.class_def.parent_class);
            if (node->data.class_def.members) {
                for (int i = 0; i < node->data.class_def.member_count; i++) {
                    if (node->data.class_def.members[i]) free_ast_node(node->data.class_def.members[i]);
                }
                free(node->data.class_def.members);
            }
            if (node->data.class_def.methods) {
                for (int i = 0; i < node->data.class_def.method_count; i++) {
                    if (node->data.class_def.methods[i]) free_ast_node(node->data.class_def.methods[i]);
                }
                free(node->data.class_def.methods);
            }
            break;
        case NODE_CONSTRUCTOR:
        case NODE_DESTRUCTOR:
            if (node->data.func.params) {
                for (int i = 0; i < node->data.func.param_count; i++) {
                    if (node->data.func.params[i]) free(node->data.func.params[i]);
                }
                free(node->data.func.params);
            }
            if (node->data.func.body) free_ast_node(node->data.func.body);
            break;
        default:
            break;
    }
    
    // Free child nodes
    if (node->left) free_ast_node(node->left);
    if (node->right) free_ast_node(node->right);
    if (node->third) free_ast_node(node->third);
    if (node->fourth) free_ast_node(node->fourth);
    
    if (node->module_name) free(node->module_name);
    free(node);
}

// ======================================================
// [SECTION] MAIN FUNCTION
// ======================================================
int main(int argc, char* argv[]) {
    // Initialize random seed for variable sizes
    srand(time(NULL));
    
    // Initialize global scope
    global_scope = create_scope(0);
    current_scope = global_scope;
    
    // Print banner
    printf(CYAN "\n");
    printf("   _____ _       __ _   _____ _                 \n");
    printf("  / ____(_)     / _| | |  __ (_)                \n");
    printf(" | (___  _ _ __| |_| |_| |__) | _____      __   \n");
    printf("  \\___ \\| | '__|  _| __|  ___/ |/ _ \\ \\ /\\ / /  \n");
    printf("  ____) | | |  | | | |_| |   | | (_) \\ V  V /   \n");
    printf(" |_____/|_|_|  |_|  \\__|_|   |_|\\___/ \\_/\\_/    \n");
    printf("                                                \n");
    printf("         %s - Fusion CLAIR & SYM              \n", SWIFTFLOW_VERSION);
    printf("         Advanced Package System                \n");
    printf("         Package path: /usr/local/lib/swift     \n");
    printf(RESET "\n");
    
    if (argc < 2) {
        // REPL mode
        repl();
    } else {
        // File mode
        Error error = {0};
        char* source = load_file(argv[1], &error);
        
        if (!source) {
            print_error(&error);
            return 1;
        }
        
        printf(GREEN "[LOAD]" RESET ">> Executing %s\n", argv[1]);
        for (int i = 0; i < 60; i++) printf("=");
        printf("\n\n");
        
        run(source, argv[1], &error);
        free(source);
        
        if (error.message[0]) {
            print_error(&error);
            return 1;
        }
    }
    
    // Final cleanup
    cleanup_variables();
    cleanup_dbvar();
    cleanup_packages();
    cleanup_classes();
    
    // Free global scope
    if (global_scope) {
        free(global_scope);
    }
    
    return 0;
}

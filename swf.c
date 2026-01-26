#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include <stdio.h>
#include "http.h"
#include "sys.h"
#include "json.h" // on déclare le prototype ici
#include "net.h"
#include "io.h"
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

// ======================================================
// [SECTION] GLOBAL STATE
// ======================================================
static char current_working_dir[PATH_MAX];
extern ASTNode** parse(const char* source, int* count);
static char* generateLambdaName();
static const char* current_exec_filename = "main";

void runtime_error(ASTNode* node, const char* fmt, ...) {
    va_list args;
    fprintf(stderr, "%s[RUNTIME ERROR] %s:%d:%d: %s", 
            COLOR_RED, current_exec_filename, node->line, node->column, COLOR_RESET);
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    exit(1); // On arrête tout proprement
}

// Fonctions IO
void io_open(ASTNode* node);
void io_close(ASTNode* node);
void io_read(ASTNode* node);
void io_write(ASTNode* node);
void io_seek(ASTNode* node);
void io_tell(ASTNode* node);
void io_exists(ASTNode* node);
void io_isfile(ASTNode* node);
void io_isdir(ASTNode* node);
void io_mkdir(ASTNode* node);
void io_listdir(ASTNode* node);
// ======================================================
// [SECTION] VERSION INFORMATION
// ======================================================
#define SWIFT_VERSION_MAJOR 1
#define SWIFT_VERSION_MINOR 0
#define SWIFT_VERSION_PATCH 0
#define SWIFT_VERSION_STRING "1.0.0"
#define SWIFT_BUILD_DATE __DATE__
#define SWIFT_BUILD_TIME __TIME__

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
    } value;
    bool is_float;
    bool is_string;
    bool is_initialized;
    bool is_constant;
    int scope_level;
    char* module;
    bool is_exported;
    bool is_locked;
} Variable;

static Variable vars[1000];
static int var_count = 0;
static int scope_level = 0;
// ======================================================
// [SECTION] EXPORT SYSTEM
// ======================================================
typedef struct {
    char* symbol;
    char* alias;
    int scope_level;
    char* module;
} ExportEntry;

static ExportEntry exports[100];
static int export_count = 0;

static void registerExport(const char* symbol, const char* alias) {
    if (export_count < 100) {
        exports[export_count].symbol = str_copy(symbol);
        exports[export_count].alias = str_copy(alias);
        exports[export_count].scope_level = scope_level;
        exports[export_count].module = str_copy(current_working_dir);
        export_count++;
        
        printf("%s[EXPORT]%s Registered export: %s as %s\n", 
               COLOR_GREEN, COLOR_RESET, symbol, alias);
    }
}

// ======================================================
// [SECTION] FUNCTION SYSTEM
// ======================================================
typedef struct {
    char name[100];
    ASTNode* params;
    ASTNode* body;
    int param_count;
    char** param_names;
    int return_scope_level;
    double return_value;
    char* return_string;
    bool has_returned;
} Function;

static Function functions[200];
static int func_count = 0;
static Function* current_function = NULL;

// ======================================================
// [SECTION] CLASS SYSTEM
// ======================================================
typedef struct {
    char name[100];
    char* parent;
    ASTNode* members;
} Class;

static Class classes[100];
static int class_count = 0;

// ======================================================
// [SECTION] IMPORT SYSTEM (ADVANCED)
// ======================================================
// ======================================================
// [SECTION] OOP RUNTIME
// ======================================================

typedef struct {
    char id[64];      // ex: "inst_1"
    char class_name[64]; // ex: "Zarch"
} InstanceRegistry;

static InstanceRegistry instances[200];
static int instance_count = 0;
static char* current_this = NULL; // Pour stocker "inst_X" lors d'un appel de méthode

// Enregistre une instance
static void registerInstance(const char* id, const char* class_name) {
    if (instance_count < 200) {
        strcpy(instances[instance_count].id, id);
        strcpy(instances[instance_count].class_name, class_name);
        instance_count++;
    }
}

// Trouve la classe d'une instance
static char* findClassOf(const char* id) {
    for(int i=0; i<instance_count; i++) {
        if(strcmp(instances[i].id, id) == 0) return instances[i].class_name;
    }
    return NULL;
}

typedef enum {
    MODULE_STATUS_NOT_LOADED,
    MODULE_STATUS_LOADING,   // Pour détecter les cycles
    MODULE_STATUS_LOADED     // Chargé et en cache
} ModuleStatus;

typedef struct {
    char* path;              // Chemin absolu unique (Clé du cache)
    char* name;              // Nom du module
    ModuleStatus status;
    int export_start_index;  // Où commencent les exports de ce module dans le tableau global
    int export_end_index;    // Où finissent les exports
} ModuleCache;

static ModuleCache module_registry[200];
static int registry_count = 0;
// ======================================================
// [SECTION] FILE I/O SYSTEM
// ======================================================
typedef struct {
    FILE* handle;
    char* name;
    char* mode;
    bool is_open;
} FileHandle;

static FileHandle open_files[50];
static int file_count = 0;

// ======================================================
// [SECTION] FUNCTION DECLARATIONS
// ======================================================
static void execute(ASTNode* node);
static double evalFloat(ASTNode* node);
static char* evalString(ASTNode* node);
static bool evalBool(ASTNode* node);
static char* weldInput(const char* prompt);
static void initWorkingDir(const char* filename);
static char* resolveImportPath(const char* import_path, const char* from_module);
static bool loadAndExecuteModule(const char* import_path, const char* from_module, bool import_named, char** named_symbols, int symbol_count);
static void showVersion();
static void showHelp();
static void executeRead(ASTNode* node);
static void executeWrite(ASTNode* node);
static void executeAppend(ASTNode* node);

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
                current_working_dir[sizeof(current_working_dir) - 1] = '\0';
                free(dir_copy);
            }
        } else {
            if (getcwd(current_working_dir, sizeof(current_working_dir)) == NULL) {
                strcpy(current_working_dir, ".");
            }
        }
    } else {
        if (getcwd(current_working_dir, sizeof(current_working_dir)) == NULL) {
            strcpy(current_working_dir, ".");
        }
    }
}

static int calculateVariableSize(TokenKind type) {
    switch (type) {
        case TK_VAR: return 8;
        case TK_NET: return 16;
        case TK_CLOG: return 32;
        case TK_DOS: return 64;
        case TK_SEL: return 128;
        default: return 8;
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

static void registerFunction(const char* name, ASTNode* params, ASTNode* body, int param_count) {
    if (func_count < 200) {
        printf("%s[REGISTER FUNC]%s register: %s (params: %d)\n", COLOR_MAGENTA, COLOR_RESET, name, param_count);
        Function* func = &functions[func_count];
        strncpy(func->name, name, 99);
        func->name[99] = '\0';
        func->params = params;
        func->body = body;
        func->param_count = param_count;
        func->has_returned = false;
        func->return_value = 0;
        func->return_string = NULL;
        func->return_scope_level = -1;
        
        if (param_count > 0) {
            func->param_names = malloc(param_count * sizeof(char*));
            ASTNode* param = params;
            int i = 0;
            while (param && i < param_count) {
                if (param->type == NODE_IDENT && param->data.name) {
                    func->param_names[i] = str_copy(param->data.name);
                } else {
                    func->param_names[i] = NULL;
                }
                param = param->right;
                i++;
            }
        } else {
            func->param_names = NULL;
        }
        
        func_count++;
        printf("%s[FUNC REG]%s Function '%s' registered (%d parameters)\n", 
               COLOR_GREEN, COLOR_RESET, name, param_count);
    }
}

static Function* findFunction(const char* name) {
    printf("%s[FIND FUNC]%s search: '%s'\n", COLOR_MAGENTA, COLOR_RESET, name);
    
    // Chercher exact
    for (int i = 0; i < func_count; i++) {
        printf("%s[FIND FUNC]%s   available func %d: '%s'\n", 
               COLOR_MAGENTA, COLOR_RESET, i, functions[i].name);
        if (strcmp(functions[i].name, name) == 0) {
            printf("%s[FIND FUNC]%s   → FOUND!\n", COLOR_GREEN, COLOR_RESET);
            return &functions[i];
        }
    }
    
    printf("%s[FIND FUNC]%s %s  =::=>→ NOT FOUND\n", COLOR_RED, COLOR_RESET, name);
    return NULL;
}

static void registerClass(const char* name, char* parent, ASTNode* members) {
    if (class_count < 100) {
        Class* cls = &classes[class_count];
        strncpy(cls->name, name, 99);
        cls->name[99] = '\0';
        cls->parent = parent ? str_copy(parent) : NULL;
        cls->members = members;
        class_count++;
        printf("%s[CLASS REG]%s Class '%s' registered\n", COLOR_MAGENTA, COLOR_RESET, name);
    }
}
static ModuleCache* findInCache(const char* absolute_path) {
    for (int i = 0; i < registry_count; i++) {
        if (strcmp(module_registry[i].path, absolute_path) == 0) {
            return &module_registry[i];
        }
    }
    return NULL;
}

static ModuleCache* addToCache(const char* absolute_path, const char* name) {
    if (registry_count >= 200) return NULL;
    
    ModuleCache* mod = &module_registry[registry_count++];
    mod->path = strdup(absolute_path);
    mod->name = strdup(name ? name : "unknown");
    mod->status = MODULE_STATUS_LOADING;
    mod->export_start_index = 0;
    mod->export_end_index = 0;
    return mod;
}

static bool isLocalImport(const char* import_path) {
    return import_path[0] == '.' || import_path[0] == '/';
}

static char* resolveModulePath(const char* import_path, const char* from_module) {
    char base_path[PATH_MAX];
    char resolved[PATH_MAX];
    
    // 1. Déterminer le dossier de base
    if (from_module && from_module[0]) {
        // Relatif au module appelant
        char temp[PATH_MAX];
        strcpy(temp, from_module);
        strcpy(base_path, dirname(temp));
    } else {
        // Relatif au dossier de travail actuel
        strcpy(base_path, current_working_dir);
    }

    // 2. Construire les chemins candidats
    char candidate[PATH_MAX];
    
    // Cas A: Import direct ou relatif
    if (import_path[0] == '/' || import_path[0] == '.') {
        snprintf(candidate, PATH_MAX, "%s/%s", base_path, import_path);
    } 
    // Cas B: Import "système" ou librairie (ex: import "mathlib")
    else {
        // Ordre de recherche : 
        // 1. ./zarch_modules/ (Standard Zarch)
        // 2. /usr/local/lib/swift/ (Global)
        // 3. ./ (Local)
        
        const char* search_paths[] = {
            "./zarch_modules",
            "/usr/local/lib/swift",
            base_path,
            NULL
        };
        
        bool found = false;
        for (int i = 0; search_paths[i]; i++) {
            snprintf(candidate, PATH_MAX, "%s/%s", search_paths[i], import_path);
            if (access(candidate, F_OK) == 0 || access(candidate, R_OK) != -1) { // Vérif dossier ou fichier
                found = true;
                break;
            }
            // Essayer avec .swf
            char candidate_ext[PATH_MAX];
            snprintf(candidate_ext, PATH_MAX, "%s.swf", candidate);
            if (access(candidate_ext, F_OK) == 0) {
                strcpy(candidate, candidate_ext);
                found = true;
                break;
            }
        }
        
        if (!found) {
            // Par défaut, on tente dans le dossier courant si rien trouvé
            snprintf(candidate, PATH_MAX, "%s/%s", base_path, import_path);
        }
    }

    // 3. Résolution Fichier vs Dossier (Package)
    struct stat path_stat;
    stat(candidate, &path_stat);
    
    if (S_ISDIR(path_stat.st_mode)) {
        // C'est un dossier ! On cherche un point d'entrée.
        // Priorité : zarch.json (si on parseait le json), sinon index.swf, main.swf
        char entry_point[PATH_MAX];
        
        // Tentative 1: index.swf
        snprintf(entry_point, PATH_MAX, "%s/index.swf", candidate);
        if (access(entry_point, F_OK) == 0) {
            if (realpath(entry_point, resolved)) return strdup(resolved);
        }
        
        // Tentative 2: main.swf
        snprintf(entry_point, PATH_MAX, "%s/main.swf", candidate);
        if (access(entry_point, F_OK) == 0) {
            if (realpath(entry_point, resolved)) return strdup(resolved);
        }
        
        // Tentative 3: Le nom du dossier.swf à l'intérieur (ex: math/math.swf)
        char *folder_name = basename(candidate);
        snprintf(entry_point, PATH_MAX, "%s/%s.swf", candidate, folder_name);
        if (access(entry_point, F_OK) == 0) {
            if (realpath(entry_point, resolved)) return strdup(resolved);
        }
    } else {
        // C'est potentiellement un fichier
        if (access(candidate, F_OK) == 0) {
             if (realpath(candidate, resolved)) return strdup(resolved);
        }
        
        // Essayer d'ajouter .swf
        char with_ext[PATH_MAX];
        snprintf(with_ext, PATH_MAX, "%s.swf", candidate);
        if (access(with_ext, F_OK) == 0) {
             if (realpath(with_ext, resolved)) return strdup(resolved);
        }
    }

    return NULL; // Non trouvé
}
// ======================================================
// [SECTION] FONCTION D'IMPORT - MODIFIÉE
// ======================================================
static bool loadAndExecuteModule(const char* import_path, const char* from_module, 
                                 bool import_named, char** named_symbols, int symbol_count) {
    if (strcmp(import_path, "sys") == 0 || strcmp(import_path, "http") == 0 || 
        strcmp(import_path, "io") == 0 || strcmp(import_path, "json") == 0 || 
        strcmp(import_path, "net") == 0 || strcmp(import_path, "std") == 0) {
        return true; 
    }
    // 1. Résoudre le chemin absolu
    char* full_path = resolveModulePath(import_path, from_module);
    if (!full_path) {
        printf("%s[IMP][ERR]%s Module not found: %s\n", COLOR_RED, COLOR_RESET, import_path);
        return false;
    }

    // 2. VÉRIFICATION DU CACHE
    ModuleCache* cache = findInCache(full_path);
    if (cache) {
        if (cache->status == MODULE_STATUS_LOADING) {
            printf("%s[IMPORT WARN]%s Circular dependency detected for %s. Breaking cycle.\n", 
                   COLOR_YELLOW, COLOR_RESET, import_path);
            // On retourne true pour ne pas planter, mais on n'exécute pas à nouveau
            // Le module appelant devra faire avec ce qui est déjà exporté
            free(full_path);
            return true;
        }
        if (cache->status == MODULE_STATUS_LOADED) {
            
            
            // On traite juste les imports nommés depuis le cache
            // (La logique de liaison des exports existants)
            // ... (Code de liaison identique à avant, voir plus bas) ...
            free(full_path);
            return true;
        }
    }

    // 3. Ajouter au cache en statut LOADING
    cache = addToCache(full_path, import_path);
    
    

    // 4. Lecture Fichier
    FILE* f = fopen(full_path, "r");
    if (!f) {
        free(full_path);
        return false;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);

    // 5. Sauvegarde contexte
    char old_dir[PATH_MAX];
    strncpy(old_dir, current_working_dir, PATH_MAX);
    char module_dir[PATH_MAX];
    strncpy(module_dir, full_path, PATH_MAX);
    strncpy(current_working_dir, dirname(module_dir), PATH_MAX);

    // 6. Parsing
    int node_count = 0;
    ASTNode** nodes = parse(source, &node_count);
    
    if (!nodes) {
        free(source); free(full_path);
        strncpy(current_working_dir, old_dir, PATH_MAX);
        return false;
    }

    // 7. Enregistrer le début des exports pour ce module
    cache->export_start_index = export_count;

    // 8. PRÉ-TRAITEMENT (Exportations)
    for (int i = 0; i < node_count; i++) {
        if (nodes[i] && nodes[i]->type == NODE_EXPORT) {
            execute(nodes[i]);
            // On marque l'export comme appartenant à ce module (optionnel pour le nettoyage futur)
            if (export_count > 0) {
                exports[export_count-1].module = strdup(full_path);
            }
        }
    }
    
    cache->export_end_index = export_count;

    // 9. EXÉCUTION DU CORPS (Initialisation)
    for (int i = 0; i < node_count; i++) {
        if (nodes[i] && nodes[i]->type != NODE_FUNC && nodes[i]->type != NODE_EXPORT && nodes[i]->type != NODE_CLASS) {
            execute(nodes[i]);
        }
    }

    // 10. Marquer comme CHARGÉ
    cache->status = MODULE_STATUS_LOADED;

    // 11. Restauration & Nettoyage
    strncpy(current_working_dir, old_dir, PATH_MAX);
    
    // Nettoyage AST (simplifié)
    for (int i = 0; i < node_count; i++) if (nodes[i]) free(nodes[i]);
    free(nodes);
    free(source);
    free(full_path);

        return true;
}


static bool isSymbolExported(const char* symbol, const char* module_path) {
    for (int i = 0; i < export_count; i++) {
        if (strcmp(exports[i].symbol, symbol) == 0 &&
            strcmp(exports[i].module, module_path) == 0) {
            return true;
        }
        if (exports[i].alias && strcmp(exports[i].alias, symbol) == 0 &&
            strcmp(exports[i].module, module_path) == 0) {
            return true;
        }
    }
    return false;
}

// ======================================================
// [SECTION] VERSION AND HELP FUNCTIONS
// ======================================================
static void showVersion() {
    printf("\n");
    printf("%s╔════════════════════════════════════════════════════════════════╗%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║                     SWIFT FLOW COMPILER                        ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║  Version: %s%-49s%s║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, SWIFT_VERSION_STRING, COLOR_CYAN, COLOR_RESET);
    printf("%s║  Build:   %s%-49s%s║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, SWIFT_BUILD_DATE " " SWIFT_BUILD_TIME, COLOR_CYAN, COLOR_RESET);
    printf("%s║  Author:  %s%-49s%s║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, "SwiftFlow Development Team", COLOR_CYAN, COLOR_RESET);
    printf("%s║  License: %s%-49s%s║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, "MIT Open Source License", COLOR_CYAN, COLOR_RESET);
    printf("%s╚════════════════════════════════════════════════════════════════╝%s\n", COLOR_CYAN, COLOR_RESET);
    printf("\n");
}

static void showHelp() {
    printf("\n");
    printf("%s╔════════════════════════════════════════════════════════════════╗%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║                   SWIFT FLOW - HELP MENU                       ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║  Usage:                                                         ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║    %sswift %s<filename.swf>%s                                     ║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, COLOR_CYAN, COLOR_CYAN, COLOR_RESET);
    printf("%s║    %sswift %s(repl mode)%s                                         ║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, COLOR_CYAN, COLOR_CYAN, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║  Options:                                                       ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║    %s--version%s       Show version information                    ║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, COLOR_CYAN, COLOR_RESET);
    printf("%s║    %s--help%s          Show this help message                     ║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, COLOR_CYAN, COLOR_RESET);
    printf("%s║    %s-h%s              Alias for --help                          ║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, COLOR_CYAN, COLOR_RESET);
    printf("%s║    %s-v%s              Alias for --version                       ║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, COLOR_CYAN, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║  Commands (in REPL):                                            ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║    %sexit%s           Exit REPL                                ║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, COLOR_CYAN, COLOR_RESET);
    printf("%s║    %sclear%s          Clear screen                             ║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, COLOR_CYAN, COLOR_RESET);
    printf("%s║    %sdbvar%s          Show all variables                       ║%s\n", COLOR_CYAN, COLOR_BRIGHT_WHITE, COLOR_CYAN, COLOR_RESET);
    printf("%s╚════════════════════════════════════════════════════════════════╝%s\n", COLOR_CYAN, COLOR_RESET);
    printf("\n");
    printf("%sLanguage Features:%s\n", COLOR_BRIGHT_CYAN, COLOR_RESET);
    printf("  • Variables: var, net, clog, dos, sel, let, const\n");
    printf("  • Control flow: if, else, while, for, for-in\n");
    printf("  • Functions: func, return\n");
    printf("  • I/O: print, weld, read, write, append\n");
    printf("  • Classes and inheritance\n");
    printf("  • Imports and modules\n");
    printf("  • JSON and data literals\n");
    printf("\n");
}

// ======================================================
// [SECTION] EXPRESSION EVALUATION
// ======================================================
static double evalFloat(ASTNode* node) {
    if (!node) return 0.0;
    
    switch (node->type) {

        // DANS evalFloat(ASTNode* node)

     // Dans swf.c, fonction evalFloat()
    case NODE_PATH_EXISTS: {
        char* path = evalString(node->left);
        // Utilise la fonction booléenne de io.c
        bool res = (access(path, F_OK) == 0); 
        if (path) free(path);
        return res ? 1.0 : 0.0;
    }   
    case NODE_AWAIT: {
        if (node->left) {
            return evalFloat(node->left);
        }
        return 0.0;
    }
        case NODE_STD_LEN: {
            // Retourne la longueur d'une string ou d'un tableau (simplifié string ici)
            char* val = evalString(node->left);
            int len = val ? strlen(val) : 0;
            if (val) free(val);
            return (double)len;
        }
        case NODE_STD_TO_INT: {
            char* val = evalString(node->left);
            double res = val ? atof(val) : 0;
            if (val) free(val);
            return res;
        }
        case NODE_SYS_EXEC: {
            char* cmd = evalString(node->left);
            // sys_exec_int est une nouvelle fonction dans sys.c qui retourne le int
            // ou on adapte sys_exec pour retourner int
            int res = system(cmd); 
            if (cmd) free(cmd);
            return (double)res;
        }
        case NODE_NET_SOCKET:
            return (double)net_socket_create();
            
        case NODE_NET_LISTEN: {
            int port = (int)evalFloat(node->left); // Évaluation de la variable port
            return (double)net_start_listen(port);
        }
            
        case NODE_NET_ACCEPT: {
            int server_fd = (int)evalFloat(node->left); // Évaluation de la variable server
            return (double)net_accept_client(server_fd);
        }
        case NODE_INT:
            return (double)node->data.int_val;
            
        case NODE_FLOAT:
            return node->data.float_val;
            
        case NODE_BOOL:
            return node->data.bool_val ? 1.0 : 0.0;
            
        case NODE_NULL:
        case NODE_UNDEFINED:
        case NODE_NAN:
            return NAN;
            
        case NODE_INF:
            return INFINITY;
            
        case NODE_STRING: {
            char* endptr;
            double val = strtod(node->data.str_val, &endptr);
            if (endptr != node->data.str_val) {
                return val;
            }
            return 0.0;
        }
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vars[idx].is_float) {
                    return vars[idx].value.float_val;
                } else if (vars[idx].is_string) {
                    char* endptr;
                    double val = strtod(vars[idx].value.str_val, &endptr);
                    if (endptr != vars[idx].value.str_val) {
                        return val;
                    }
                    return 0.0;
                } else {
                    return (double)vars[idx].value.int_val;
                }
            }
            printf("%s[EXEC ERROR]%s Undefined variable: %s\n", COLOR_RED, COLOR_RESET, node->data.name);
            return 0.0;
        }
            
        case NODE_BINARY: {
            double left = evalFloat(node->left);
            double right = evalFloat(node->right);
            
            switch (node->op_type) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: 
                    if (right == 0.0) {
                        printf("%s[EXEC WARNING]%s Division by zero\n", COLOR_YELLOW, COLOR_RESET);
                        return INFINITY;
                    }
                    return left / right;
                case TK_MOD: 
                    if (right == 0.0) {
                        printf("%s[EXEC WARNING]%s Modulo by zero\n", COLOR_YELLOW, COLOR_RESET);
                        return 0.0;
                    }
                    return fmod(left, right);
                case TK_POW: return pow(left, right);
                case TK_CONCAT: {
                    char* left_str = evalString(node->left);
                    char* right_str = evalString(node->right);
                    char* combined = malloc(strlen(left_str) + strlen(right_str) + 1);
                    strcpy(combined, left_str);
                    strcat(combined, right_str);
                    
                    char* endptr;
                    double val = strtod(combined, &endptr);
                    
                    free(left_str);
                    free(right_str);
                    free(combined);
                    
                    if (endptr != combined) return val;
                    return 0.0;
                }
                case TK_EQ: return left == right ? 1.0 : 0.0;
                case TK_NEQ: return left != right ? 1.0 : 0.0;
                case TK_GT: return left > right ? 1.0 : 0.0;
                case TK_LT: return left < right ? 1.0 : 0.0;
                case TK_GTE: return left >= right ? 1.0 : 0.0;
                case TK_LTE: return left <= right ? 1.0 : 0.0;
                case TK_AND: return (left != 0.0 && right != 0.0) ? 1.0 : 0.0;
                case TK_OR: return (left != 0.0 || right != 0.0) ? 1.0 : 0.0;
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
            
        case NODE_TERNARY: {
            double condition = evalFloat(node->left);
            if (condition != 0.0) {
                return evalFloat(node->right);
            } else {
                return evalFloat(node->third);
            }
        }
            
        case NODE_FUNC_CALL: {
            Function* func = findFunction(node->data.name);
            if (func) {
                Function* prev_func = current_function;
                current_function = func;
                
                int old_scope = scope_level;
                scope_level++;
                
                if (node->left && func->param_names) {
                    ASTNode* arg = node->left;
                    int param_idx = 0;
                    
                    while (arg && param_idx < func->param_count) {
                        if (func->param_names[param_idx]) {
                            if (var_count < 1000) {
                                Variable* var = &vars[var_count];
                                strncpy(var->name, func->param_names[param_idx], 99);
                                var->name[99] = '\0';
                                var->type = TK_VAR;
                                var->size_bytes = 8;
                                var->scope_level = scope_level;
                                var->is_constant = false;
                                var->is_initialized = true;
                                
                                double arg_val = evalFloat(arg);
                                var->is_float = true;
                                var->is_string = false;
                                var->value.float_val = arg_val;
                                
                                var_count++;
                            }
                        }
                        arg = arg->right;
                        param_idx++;
                    }
                }
                
                func->has_returned = false;
                func->return_value = 0;
                if (func->return_string) {
                    free(func->return_string);
                    func->return_string = NULL;
                }
                
                if (func->body) {
                    execute(func->body);
                }
                
                scope_level = old_scope;
                current_function = prev_func;
                
                if (func->return_string) {
                    char* endptr;
                    double val = strtod(func->return_string, &endptr);
                    if (endptr != func->return_string) {
                        return val;
                    }
                }
                return func->return_value;
            }
            
            printf("%s[EXEC ERROR]%s Function not found: %s\n", COLOR_RED, COLOR_RESET, node->data.name);
            return 0.0;
        }
            
        default:
            return 0.0;
    }
}

static char* evalString(ASTNode* node) {
    if (!node) return str_copy("");
    
    switch (node->type) {
    
    // --- INSTANCIATION ---
    case NODE_NEW: {
        static int instance_id = 0;
        char instance_name[64];
        sprintf(instance_name, "inst_%d", ++instance_id);
        
        // On lie l'instance à sa classe
        registerInstance(instance_name, node->data.name);
        
        return str_copy(instance_name);
    }

    // --- ACCÈS MEMBRE ---
    case NODE_MEMBER_ACCESS: {
        char* obj_name = NULL;
        
        if (node->left->type == NODE_THIS) {
            if (current_this) obj_name = str_copy(current_this);
            else return str_copy("");
        } else {
            obj_name = evalString(node->left);
        }
        
        char* prop_name = node->right->data.name;
        
        if (!obj_name) return str_copy("");

        char full_name[256];
        snprintf(full_name, 256, "%s_%s", obj_name, prop_name);
        free(obj_name);

        int idx = findVar(full_name);
        if (idx >= 0 && vars[idx].is_string) {
            return str_copy(vars[idx].value.str_val);
        }
        return str_copy("");
    }

    // --- IO ---
    case NODE_FILE_READ: {
        char* path = evalString(node->left);
        char* content = io_read_string(path); 
        if (path) free(path);
        return content ? content : str_copy("");
    }

    case NODE_WELD: {
        char* prompt = NULL;
        if (node->left) {
            prompt = evalString(node->left);
        }
        char* input = weldInput(prompt);
        if (prompt) free(prompt);
        return input ? input : str_copy("");
    }

    // --- STD ---
    case NODE_STD_TO_STR: {
        double val = evalFloat(node->left);
        char* buf = malloc(64);
        if (val == (int64_t)val) sprintf(buf, "%lld", (int64_t)val);
        else sprintf(buf, "%g", val);
        return buf;
    }

    case NODE_STD_SPLIT: {
        char* str = evalString(node->left);
        char* delim = evalString(node->right);
        char* token = strtok(str, delim);
        char* res = token ? str_copy(token) : str_copy("");
        if (str) free(str);
        if (delim) free(delim);
        return res;
    }

    // --- LAMBDA ---
    case NODE_LAMBDA: {
        char* anon_name = generateLambdaName();
        int param_count = 0;
        ASTNode* param = node->left;
        while (param) {
            param_count++;
            param = param->right;
        }
        registerFunction(anon_name, node->left, node->right, param_count);
        return anon_name;
    }

    // --- AWAIT ---
    case NODE_AWAIT: {
        if (node->left) {
            return evalString(node->left);
        }
        return str_copy("");
    }

    // --- HTTP & JSON & SYS ---
    case NODE_HTTP_GET: {
        char* url = evalString(node->left);
        char* res = http_get(url);
        if (url) free(url);
        return res ? res : str_copy("");
    }
    case NODE_HTTP_POST: {
        char* url = evalString(node->left);
        char* data = evalString(node->right);
        char* res = http_post(url, data);
        if (url) free(url);
        if (data) free(data);
        return res ? res : str_copy("");
    }
    case NODE_HTTP_DOWNLOAD: {
        char* url = evalString(node->left);
        char* out = evalString(node->right);
        char* res = http_download(url, out);
        if (url) free(url);
        if (out) free(out);
        return res ? res : str_copy("failed");
    }
    case NODE_SYS_ARGV: {
        int idx = (int)evalFloat(node->left);
        char* arg = sys_get_argv(idx); 
        return arg ? str_copy(arg) : NULL;
    }
    case NODE_JSON_GET: {
        char* json = evalString(node->left);
        char* key = evalString(node->right);
        char* res = json_extract(json, key);
        if (json) free(json);
        if (key) free(key);
        return res ? res : NULL;
    }

    // --- NET ---
    case NODE_NET_RECV: {
        int fd = (int)evalFloat(node->left);
        int size = 1024;
        if (node->right) size = (int)evalFloat(node->right);
        char* res = net_recv_data(fd, size);
        if (res) return res;
        return str_copy("");
    }

    // --- BASIC TYPES ---
    case NODE_STRING:
        return str_copy(node->data.str_val);
        
    case NODE_INT: {
        char* result = malloc(32);
        if (result) sprintf(result, "%lld", node->data.int_val);
        return result ? result : str_copy("");
    }
        
    case NODE_FLOAT: {
        char* result = malloc(32);
        if (result) {
            double val = node->data.float_val;
            if (isnan(val)) strcpy(result, "nan");
            else if (isinf(val)) strcpy(result, val > 0 ? "inf" : "-inf");
            else if (fabs(val - (int64_t)val) < 1e-10) sprintf(result, "%lld", (int64_t)val);
            else sprintf(result, "%g", val);
        }
        return result ? result : str_copy("");
    }
        
    case NODE_BOOL:
        return str_copy(node->data.bool_val ? "true" : "false");
        
    case NODE_NULL:
        return str_copy("null");
        
    case NODE_UNDEFINED:
        return str_copy("undefined");
        
    case NODE_IDENT: {
        int idx = findVar(node->data.name);
        if (idx >= 0) {
            if (vars[idx].is_string && vars[idx].value.str_val) {
                return str_copy(vars[idx].value.str_val);
            } else if (vars[idx].is_float) {
                char* result = malloc(32);
                if (result) {
                    double val = vars[idx].value.float_val;
                    if (fabs(val - (int64_t)val) < 1e-10) sprintf(result, "%lld", (int64_t)val);
                    else sprintf(result, "%g", val);
                }
                return result ? result : str_copy("");
            } else {
                char* result = malloc(32);
                if (result) sprintf(result, "%lld", vars[idx].value.int_val);
                return result ? result : str_copy("");
            }
        } else {
            return str_copy("undefined");
        }
    }
        
    case NODE_BINARY: {
        if (node->op_type == TK_CONCAT) {
            char* left_str = evalString(node->left);
            char* right_str = evalString(node->right);
            char* result = malloc(strlen(left_str) + strlen(right_str) + 1);
            if (result) {
                strcpy(result, left_str);
                strcat(result, right_str);
            }
            free(left_str);
            free(right_str);
            return result ? result : str_copy("");
        } else {
            double val = evalFloat(node);
            char* result = malloc(32);
            if (result) sprintf(result, "%g", val);
            return result ? result : str_copy("");
        }
    }
        
    // --- APPEL DE FONCTION & METHODE ---
    case NODE_FUNC_CALL: {
        char* func_name = node->data.name;
        char* prev_this = current_this; 
        
        char* dot = strchr(func_name, '.');
        char real_func_name[256];
        bool is_method = false;

        if (dot) {
            int len = dot - func_name;
            char var_name[128];
            strncpy(var_name, func_name, len);
            var_name[len] = '\0';
            char* method = dot + 1;
            
            int idx = findVar(var_name);
            if (idx >= 0 && vars[idx].is_string) {
                char* instance_id = vars[idx].value.str_val;
                char* cls = findClassOf(instance_id);
                if (cls) {
                    snprintf(real_func_name, 256, "%s_%s", cls, method);
                    func_name = real_func_name;
                    current_this = instance_id;
                    is_method = true;
                }
            }
        }

        Function* func = findFunction(func_name);
        
        if (func) {
            Function* prev_func = current_function;
            current_function = func;
            int old_scope = scope_level;
            scope_level++;
            
            if (node->left && func->param_names) {
                ASTNode* arg = node->left;
                int param_idx = 0;
                while (arg && param_idx < func->param_count) {
                    if (func->param_names[param_idx]) {
                        if (var_count < 1000) {
                            Variable* var = &vars[var_count];
                            strncpy(var->name, func->param_names[param_idx], 99);
                            var->type = TK_VAR;
                            var->scope_level = scope_level;
                            var->is_initialized = true;
                            
                            if (arg->type == NODE_STRING) {
                                var->is_string = true;
                                var->value.str_val = evalString(arg);
                            } else {
                                var->is_float = true;
                                var->value.float_val = evalFloat(arg);
                            }
                            var_count++;
                        }
                    }
                    arg = arg->right;
                    param_idx++;
                }
            }
            
            func->has_returned = false;
            if (func->body) execute(func->body);
            
            scope_level = old_scope;
            current_function = prev_func;
            if (is_method) current_this = prev_this;
            
            if (func->return_string) {
                return str_copy(func->return_string);
            } else {
                char buf[64];
                sprintf(buf, "%g", func->return_value);
                return str_copy(buf);
            }
        }
        
        if (is_method) current_this = prev_this;
        return str_copy("");
    } // Fin NODE_FUNC_CALL

    default:
        return str_copy("");
    } // Fin SWITCH
    
    return str_copy("");
} // Fin FONCTION evalString
    
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
        case NODE_NAN:
            return false;
            
        case NODE_INF:
            return true;
            
        default:
            return evalFloat(node) != 0.0;
    }
}

// ======================================================
// [SECTION] FILE I/O FUNCTIONS
// ======================================================
static void executeRead(ASTNode* node) {
    if (!node->left) {
        printf("%s[READ ERROR]%s Missing filename\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* filename = evalString(node->left);
    if (!filename) {
        printf("%s[READ ERROR]%s Invalid filename\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    // Check if file exists
    if (access(filename, F_OK) != 0) {
        printf("%s[READ ERROR]%s File not found: %s\n", COLOR_RED, COLOR_RESET, filename);
        free(filename);
        return;
    }
    
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("%s[READ ERROR]%s Cannot open file: %s\n", COLOR_RED, COLOR_RESET, filename);
        free(filename);
        return;
    }
    
    // Read file content
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* content = malloc(size + 1);
    if (!content) {
        printf("%s[READ ERROR]%s Memory allocation failed\n", COLOR_RED, COLOR_RESET);
        fclose(f);
        free(filename);
        return;
    }
    
    fread(content, 1, size, f);
    content[size] = '\0';
    fclose(f);
    
    // Store in variable if specified
    if (node->right) {
        char* var_name = evalString(node->right);
        if (var_name) {
            int idx = findVar(var_name);
            if (idx == -1 && var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, var_name, 99);
                var->name[99] = '\0';
                var->type = TK_VAR;
                var->size_bytes = strlen(content) + 1;
                var->scope_level = scope_level;
                var->is_constant = false;
                var->is_initialized = true;
                var->is_string = true;
                var->is_float = false;
                var->value.str_val = str_copy(content);
                var_count++;
                printf("%s[READ]%s Stored file content in variable '%s'\n", COLOR_GREEN, COLOR_RESET, var_name);
            } else if (idx >= 0) {
                if (vars[idx].value.str_val) free(vars[idx].value.str_val);
                vars[idx].value.str_val = str_copy(content);
                vars[idx].is_string = true;
                vars[idx].is_initialized = true;
                printf("%s[READ]%s Updated variable '%s' with file content\n", COLOR_GREEN, COLOR_RESET, var_name);
            }
            free(var_name);
        }
    } else {
        // Store in default variable
        int idx = findVar("__file_content__");
        if (idx == -1 && var_count < 1000) {
            Variable* var = &vars[var_count];
            strcpy(var->name, "__file_content__");
            var->type = TK_VAR;
            var->size_bytes = strlen(content) + 1;
            var->scope_level = scope_level;
            var->is_constant = false;
            var->is_initialized = true;
            var->is_string = true;
            var->is_float = false;
            var->value.str_val = str_copy(content);
            var_count++;
        } else if (idx >= 0) {
            if (vars[idx].value.str_val) free(vars[idx].value.str_val);
            vars[idx].value.str_val = str_copy(content);
            vars[idx].is_initialized = true;
        }
        printf("%s[READ]%s Read %ld bytes from: %s\n", COLOR_GREEN, COLOR_RESET, size, filename);
    }
    
    free(content);
    free(filename);
}

static void executeWrite(ASTNode* node) {
    if (!node->left || !node->right) {
        printf("%s[WRITE ERROR]%s Missing filename or data\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* filename = evalString(node->left);
    char* data = evalString(node->right);
    
    if (!filename || !data) {
        if (filename) free(filename);
        if (data) free(data);
        return;
    }
    
    char* mode = "w"; // Default write mode
    if (node->third) {
        char* mode_str = evalString(node->third);
        if (mode_str && (strcmp(mode_str, "a") == 0 || strcmp(mode_str, "append") == 0)) {
            mode = "a";
        }
        free(mode_str);
    }
    
    FILE* f = fopen(filename, mode);
    if (!f) {
        printf("%s[WRITE ERROR]%s Cannot open file for writing: %s\n", COLOR_RED, COLOR_RESET, filename);
        free(filename);
        free(data);
        return;
    }
    
    size_t written = fwrite(data, 1, strlen(data), f);
    fclose(f);
    
    const char* mode_desc = (mode[0] == 'a') ? "appended to" : "written to";
    printf("%s[WRITE]%s %zu bytes %s: %s\n", COLOR_GREEN, COLOR_RESET, written, mode_desc, filename);
    
    free(filename);
    free(data);
}

static void executeAppend(ASTNode* node) {
    if (!node->left || !node->right) {
        printf("%s[APPEND ERROR]%s Missing filename or data\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* filename = evalString(node->left);
    char* data = evalString(node->right);
    
    if (!filename || !data) {
        if (filename) free(filename);
        if (data) free(data);
        return;
    }
    
    FILE* f = fopen(filename, "a");
    if (!f) {
        printf("%s[APPEND ERROR]%s Cannot open file for appending: %s\n", COLOR_RED, COLOR_RESET, filename);
        free(filename);
        free(data);
        return;
    }
    
    size_t written = fwrite(data, 1, strlen(data), f);
    fclose(f);
    
    printf("%s[APPEND]%s %zu bytes appended to: %s\n", COLOR_GREEN, COLOR_RESET, written, filename);
    
    free(filename);
    free(data);
}

// ======================================================
// [SECTION] WELD FUNCTION
// ======================================================
static char* weldInput(const char* prompt) {
    if (prompt) {
        printf("%s", prompt);
        fflush(stdout);
    }
    
    char* input = malloc(1024);
    if (input) {
        if (fgets(input, 1024, stdin)) {
            input[strcspn(input, "\n")] = 0;
        } else {
            strcpy(input, "");
        }
    }
    return input;
}
// Helper pour enregistrer une constante (utilisé par ENUM)
static void registerGlobalConstant(const char* name, int value) {
    if (var_count < 1000) {
        Variable* var = &vars[var_count];
        strncpy(var->name, name, 99);
        var->type = TK_CONST;
        var->size_bytes = 8;
        var->scope_level = 0; // Toujours global
        var->is_constant = true;
        var->is_initialized = true;
        var->is_float = false;
        var->is_string = false;
        var->value.int_val = value;
        var_count++;
        printf("%s[ENUM]%s Registered %s = %d\n", COLOR_MAGENTA, COLOR_RESET, name, value);
    }
}

// Helper pour générer un nom unique pour les lambdas
static char* generateLambdaName() {
    static int lambda_id = 0;
    char* name = malloc(32);
    sprintf(name, "__lambda_%d", lambda_id++);
    return name;
}
// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        // Dans execute, ajoute :
        case NODE_SYS_EXIT: {
            int code = 0;
            if (node->left) code = (int)evalFloat(node->left);
            exit(code);
            break;
        }
        case NODE_SYS_EXEC: {
             // Si utilisé comme instruction simple sans récupération de variable
             char* cmd = evalString(node->left);
             system(cmd);
             if (cmd) free(cmd);
             break;
        }
        case NODE_EXPORT: {
        
    if (node->data.export.symbol) {
        char* symbol = node->data.export.symbol;
        char* alias = node->data.export.alias ? node->data.export.alias : symbol;
                
        registerExport(symbol, alias);
    }
    break;
}
        // net pour le command void
        case NODE_NET_CONNECT: {
            int fd = (int)evalFloat(node->left);      // Évaluation variable sock
            char* ip = evalString(node->right);       // Évaluation string ip
            int port = (int)evalFloat(node->third);   // Évaluation int/var port
            
            net_connect_to(fd, ip, port);
            
            if (ip) free(ip);
            break;
        }
        case NODE_NET_SEND: {
            int fd = (int)evalFloat(node->left);
            char* data = evalString(node->right);
            
            net_send_data(fd, data);
            
            if (data) free(data);
            break;
        }
        case NODE_NET_CLOSE: {
            int fd = (int)evalFloat(node->left);
            net_close_socket(fd);
            break;
        }


        
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL: {
            // 1. DÉTERMINER LE TYPE
            TokenKind var_type = TK_VAR;
            if (node->type == NODE_NET_DECL) var_type = TK_NET;
            else if (node->type == NODE_CLOG_DECL) var_type = TK_CLOG;
            else if (node->type == NODE_DOS_DECL) var_type = TK_DOS;
            else if (node->type == NODE_SEL_DECL) var_type = TK_SEL;
            else if (node->type == NODE_CONST_DECL) var_type = TK_CONST;
            
            // 2. CALCULER LA VALEUR D'ABORD (IMPORTANT : AVANT d'allouer la variable)
            // Cela empêche les fonctions appelées d'écraser la mémoire de cette variable
            bool has_init = false;
            bool val_is_str = false;
            bool val_is_float = false;
            char* temp_str = NULL;
            double temp_float = 0.0;
            int64_t temp_int = 0;
            
            if (node->left) {
                has_init = true;
                if (node->left->type == NODE_STRING) {
                    val_is_str = true;
                    temp_str = str_copy(node->left->data.str_val);
                } 
                else if (node->left->type == NODE_FLOAT) {
                    val_is_float = true;
                    temp_float = node->left->data.float_val;
                }
                else if (node->left->type == NODE_BOOL) {
                    temp_int = node->left->data.bool_val ? 1 : 0;
                }
                else {
                    // Pour les appels de fonction ou expressions
                    // On exécute l'expression MAINTENANT
                    temp_float = evalFloat(node->left);
                    
                    // Si c'était un appel de fonction qui a renvoyé une string
                    if (node->left->type == NODE_FUNC_CALL) {
                        Function* f = findFunction(node->left->data.name);
                        if (f && f->return_string) {
                            val_is_str = true;
                            temp_str = str_copy(f->return_string);
                        } else {
                            val_is_float = true; // On assume float par défaut pour les retours
                        }
                    } else {
                        // Expression mathématique standard
                        val_is_float = true; 
                    }
                }
            }
            
            // 3. MAINTENANT ON ALLOUE LA VARIABLE (Une fois que l'exécution est finie)
            if (var_count < 1000) {
                Variable* var = &vars[var_count]; // On prend le slot
                
                strncpy(var->name, node->data.name, 99);
                var->name[99] = '\0';
                var->type = var_type;
                var->size_bytes = calculateVariableSize(var_type);
                var->scope_level = scope_level;
                var->is_constant = (var_type == TK_CONST);
                var->module = NULL;
                var->is_exported = false;
                
                if (has_init) {
                    var->is_initialized = true;
                    if (val_is_str) {
                        var->is_string = true;
                        var->is_float = false;
                        var->value.str_val = temp_str;
                    } else if (val_is_float) {
                        var->is_float = true;
                        var->is_string = false;
                        var->value.float_val = temp_float;
                    } else {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = (int64_t)(val_is_float ? temp_float : temp_int);
                    }
                } else {
                    var->is_initialized = false;
                    var->is_float = false;
                    var->is_string = false;
                    var->value.int_val = 0;
                }
                
                var_count++; // On incrémente le compteur SEULEMENT à la fin
            }
            break;
        }
        

case NODE_FILE_OPEN:
    io_open(node);
    break;
    
case NODE_FILE_CLOSE:
    io_close(node);
    break;
    

case NODE_FILE_WRITE:
    io_write(node);
    break;
    
case NODE_FILE_SEEK:
    io_seek(node);
    break;
    
case NODE_FILE_TELL:
    io_tell(node);
    break;
    
case NODE_PATH_ISFILE:
    io_isfile(node);
    break;
    
case NODE_PATH_ISDIR:
    io_isdir(node);
    break;
    
case NODE_DIR_CREATE:
    io_mkdir(node);
    break;
    
case NODE_DIR_LIST:
    io_listdir(node);
    break;    
        
        case NODE_ASSIGN: {
        char* target_name = NULL;
        bool is_prop = false;
        
        // 1. IDENTIFICATION DE LA CIBLE
        // Cas A : Assignation simple (x = 1)
        if (node->data.name) {
            target_name = strdup(node->data.name);
        }
        // Cas B : Assignation de propriété (obj.x = 1)
        else if (node->left && node->left->type == NODE_MEMBER_ACCESS) {
            is_prop = true;
            // On évalue la partie gauche (ex: "zarch" ou "inst_1")
            char* obj = evalString(node->left->left);
            // On prend le nom de la propriété à droite
            char* prop = node->left->right->data.name;
            
            if (obj && prop) {
                target_name = malloc(strlen(obj) + strlen(prop) + 2);
                snprintf(target_name, 256, "%s_%s", obj, prop);
            }
            if (obj) free(obj);
        }

        if (target_name) {
            // 2. RECHERCHE OU CRÉATION
            int idx = findVar(target_name);
            
            // Si la variable n'existe pas, on la crée (Auto-déclaration)
            // C'est CRUCIAL pour les propriétés d'objets qui sont créées à la volée
            if (idx == -1 && var_count < 1000) {
                idx = var_count++;
                strncpy(vars[idx].name, target_name, 99);
                vars[idx].name[99] = '\0';
                vars[idx].type = TK_VAR; 
                // Les propriétés sont globales (attachées à l'instance), les vars locales respectent le scope
                vars[idx].scope_level = (is_prop ? 0 : scope_level); 
                vars[idx].is_constant = false;
                vars[idx].is_initialized = false;
                // printf("%s[EXEC]%s Auto-declared variable/prop '%s'\n", COLOR_CYAN, COLOR_RESET, target_name);
            }

            // 3. AFFECTATION DE LA VALEUR
            if (idx >= 0) {
                if (vars[idx].is_constant) {
                      runtime_error(node, "Cannot assign to constant '%s'", target_name);
                } 
                else if (node->right) {
                    vars[idx].is_initialized = true;

                    // Nettoyage de l'ancienne valeur si c'était une chaîne
                    if (vars[idx].is_string && vars[idx].value.str_val) {
                        free(vars[idx].value.str_val);
                        vars[idx].value.str_val = NULL;
                    }

                    // --- Gestion des Types ---
                    
                    // TYPE STRING
                    if (node->right->type == NODE_STRING) {
                        vars[idx].is_string = true;
                        vars[idx].is_float = false;
                        vars[idx].value.str_val = evalString(node->right);
                    }
                    // TYPE NOUVELLE INSTANCE (new Class)
                    else if (node->right->type == NODE_NEW) {
                        vars[idx].is_string = true;
                        vars[idx].is_float = false;
                        vars[idx].value.str_val = evalString(node->right); // Retourne "inst_X"
                    }
                    // TYPES DYNAMIQUES (Appel de fonction, variable, membre)
                    else if (node->right->type == NODE_FUNC_CALL || 
                             node->right->type == NODE_IDENT || 
                             node->right->type == NODE_MEMBER_ACCESS) {
                        
                        // On évalue en string pour voir si c'est du texte ou une instance
                        char* val_str = evalString(node->right);
                        if (vars[idx].is_locked) {
                            runtime_error(node, "Cannot assign to constant '%s'", target_name);
                             return;
                        }
                        // Détection simple : Si ça ressemble à une instance "inst_" ou contient des lettres
                        // qui ne sont pas des notations scientifiques, c'est une string.
                        bool is_numeric = true;
                        if (strstr(val_str, "inst_")) is_numeric = false;
                        else {
                            char* endptr;
                            strtod(val_str, &endptr);
                            if (*endptr != '\0') is_numeric = false;
                        }

                        if (!is_numeric) {
                            vars[idx].is_string = true;
                            vars[idx].is_float = false;
                            vars[idx].value.str_val = val_str;
                        } else {
                            free(val_str);
                            vars[idx].is_string = false;
                            vars[idx].is_float = true;
                            vars[idx].value.float_val = evalFloat(node->right);
                        }
                    }
                    // TYPE NUMERIQUE / BOOL (Par défaut)
                    else {
                        vars[idx].is_string = false;
                        vars[idx].is_float = true;
                        vars[idx].value.float_val = evalFloat(node->right);
                    }
                }
            }
            free(target_name);
        }
        break;
    }
        case NODE_PRINT: {
            if (node->left) {
                ASTNode* current_arg = node->left;
                while (current_arg) {
                    char* str = evalString(current_arg);
                    printf("%s", str);
                    free(str);
                    current_arg = current_arg->right;
                    if (current_arg) printf(" ");
                }
            }
            printf("\n");
            break;
        }
            
        case NODE_READ:
            executeRead(node);
            break;
            
        case NODE_WRITE:
            executeWrite(node);
            break;
            
        case NODE_APPEND:
            executeAppend(node);
            break;
            
        case NODE_PASS:
            break;
            
        case NODE_RETURN: {
    if (current_function) {
        current_function->has_returned = true;
        if (node->left) {
            current_function->return_value = evalFloat(node->left);
            char* str_val = evalString(node->left);
            if (current_function->return_string) {
                free(current_function->return_string);
            }
            current_function->return_string = str_copy(str_val);
            free(str_val);
        } else {
            current_function->return_value = 0;
            if (current_function->return_string) {
                free(current_function->return_string);
                current_function->return_string = NULL;
            }
        }
    }
    break;
   }
            
        case NODE_BLOCK: {
            int old_scope = scope_level;
            scope_level++;
            
            ASTNode* current = node->left;
            while (current && !(current_function && current_function->has_returned)) {
                execute(current);
                current = current->right;
            }
            
            scope_level = old_scope;
            break;
        }
            
        case NODE_MAIN: {
            if (node->left) execute(node->left);
            break;
        }
            
        case NODE_DBVAR: {
            printf("\n%s╔═════════════════════════════════════════════════╗%s\n", 
                   COLOR_CYAN, COLOR_RESET);
            printf("%s║                   VARIABLE TABLE (dbvar)          ║%s\n", 
                   COLOR_CYAN, COLOR_RESET);
            printf("%s╠═══════════════════════════════════════════════════╣%s\n", 
                   COLOR_CYAN, COLOR_RESET);
            printf("%s║  Type    │ Name     │ Size │ Value  │ Initialized ║%s\n", 
                   COLOR_CYAN, COLOR_RESET);
            printf("%s╠═══════════════════════════════════════════════════╣%s\n", 
                   COLOR_CYAN, COLOR_RESET);
            
            for (int i = 0; i < var_count; i++) {
                Variable* var = &vars[i];
                char value_str[50];
                
                if (var->is_string && var->value.str_val) {
                    snprintf(value_str, sizeof(value_str), "\"%s\"", var->value.str_val);
                } else if (var->is_float) {
                    snprintf(value_str, sizeof(value_str), "%g", var->value.float_val);
                } else {
                    snprintf(value_str, sizeof(value_str), "%lld", var->value.int_val);
                }
                
                printf("%s║ %-8s │ %-11s │ %-8d │ %-11s │ %-11s ║%s\n",
                       COLOR_CYAN,
                       getTypeName(var->type),
                       var->name,
                       var->size_bytes,
                       value_str,
                       var->is_initialized ? "✓" : "✗",
                       COLOR_RESET);
            }
            
            if (var_count == 0) {
                printf("%s║                   No variables declared                       ║%s\n", 
                       COLOR_CYAN, COLOR_RESET);
            }
            
            printf("   %s╚════════════════════════════════════════════════════════════════╝%s\n", 
                   COLOR_CYAN, COLOR_RESET);
            break;
        }
            
        case NODE_IMPORT: {
    if (node->data.imports.module_count > 0) {
        for (int i = 0; i < node->data.imports.module_count; i++) {
            char* module_name = node->data.imports.modules[i];
            char* from_module = node->data.imports.from_module;
            
            // Si c'est un import nommé (import {add, PI} from "math")
            if (node->left) {
                // Compter combien de symboles sont demandés
                int symbol_count = 0;
                ASTNode* symbol_node = node->left;
                while (symbol_node) {
                    symbol_count++;
                    symbol_node = symbol_node->right;
                }
                
                // Créer un tableau des noms de symboles
                char** named_symbols = malloc(symbol_count * sizeof(char*));
                symbol_node = node->left;
                int idx = 0;
                while (symbol_node && idx < symbol_count) {
                    if (symbol_node->type == NODE_IDENT && symbol_node->data.name) {
                        named_symbols[idx] = str_copy(symbol_node->data.name);
                    } else {
                        named_symbols[idx] = NULL;
                    }
                    symbol_node = symbol_node->right;
                    idx++;
                }
                
                // Importer avec les symboles nommés
                if (!loadAndExecuteModule(module_name, from_module, true, named_symbols, symbol_count)) {
                    runtime_error(node, "Cannot import '%s'", module_name);
                }
                
                // Nettoyer
                for (int j = 0; j < symbol_count; j++) {
                    if (named_symbols[j]) free(named_symbols[j]);
                }
                free(named_symbols);
            } 
            // Import simple (import "math")
            else {
                if (!loadAndExecuteModule(module_name, from_module, false, NULL, 0)) {
                     runtime_error(node, "import runtime stoped '%s'", module_name);                          
                }
            }
        }
    }
    break;
}
   

    // --- CONTROL FLOW (IF / ELIF / ELSE) ---
    case NODE_IF: {
        bool cond = evalBool(node->left);
        if (cond) {
            execute(node->right); // Bloc IF
        } else if (node->third) {
            // Le parser gère le ELIF comme un NODE_IF imbriqué dans le 'third' (else)
            // Donc execute(node->third) gère récursivement les chaines if/else if/else
            execute(node->third);
        }
        break;
    }

    // --- LOOPS (WHILE) ---
    case NODE_WHILE: {
        // Support basique des boucles
        int safety_count = 0;
        while (evalBool(node->left)) {
            execute(node->right);
            
            // Vérification de retour de fonction (pour sortir si 'return' est appelé dans la boucle)
            if (current_function && current_function->has_returned) break;
            
            // Sécurité boucle infinie (optionnel, mis à 1M itérations)
            safety_count++;
            if (safety_count > 1000000) {
                runtime_error(node, "Cannot while func '%s'", current_function);
                break;
            }
        }
        break;
    }

    // --- LOOPS (FOR) ---
    case NODE_FOR: {
        // Initialisation (ex: var i = 0)
        if (node->data.loop.init) execute(node->data.loop.init);
        
        // Condition & Boucle
        while (evalBool(node->data.loop.condition)) {
            // Corps de la boucle
            execute(node->data.loop.body);
            
            if (current_function && current_function->has_returned) break;
            
            // Mise à jour (ex: i = i + 1)
            if (node->data.loop.update) execute(node->data.loop.update);
        }
        break;
    }

    // --- OOP (CLASS) ---
    case NODE_CLASS: {
        char* cls_name = node->data.class_def.name;
        if (cls_name) {
            // 1. Enregistrement de la classe (existant)
            char* parent = node->data.class_def.parent ? node->data.class_def.parent->data.name : NULL;
            registerClass(cls_name, parent, node->data.class_def.members);
            
            // 2. [NOUVEAU] Aplatissement des méthodes : Zarch.install -> Zarch_install
            ASTNode* member = node->data.class_def.members;
            while (member) {
                if (member->type == NODE_FUNC) {
                    char method_full_name[256];
                    snprintf(method_full_name, 256, "%s_%s", cls_name, member->data.name);
                    
                    // Compter les params
                    int p_count = 0;
                    ASTNode* p = member->left;
                    while(p) { p_count++; p = p->right; }
                    
                    registerFunction(method_full_name, member->left, member->right, p_count);
                    // printf("[OOP] Registered method: %s\n", method_full_name);
                }
                member = member->right;
            }
        }
        break;
    }

    // --- OOP (ENUM) ---
    case NODE_ENUM: {
        // Transforme: enum Color { RED, BLUE } 
        // En constantes globales: Color_RED = 0, Color_BLUE = 1
        if (node->data.name && node->left) {
            ASTNode* variant = node->left;
            int val_counter = 0;
            
            while (variant) {
                char full_name[256];
                char* variant_name = NULL;
                
                // Le variant peut être un IDENT ou un ASSIGN (si valeur manuelle)
                if (variant->type == NODE_IDENT) {
                    variant_name = variant->data.name;
                } else if (variant->type == NODE_ASSIGN && variant->data.name) {
                    variant_name = variant->data.name;
                    // Si une valeur est assignée (ex: RED = 5)
                    if (variant->left) val_counter = (int)evalFloat(variant->left);
                }
                
                if (variant_name) {
                    snprintf(full_name, 256, "%s_%s", node->data.name, variant_name);
                    registerGlobalConstant(full_name, val_counter);
                    val_counter++;
                }
                
                variant = variant->right;
            }
        }
        break;
    }

    // --- ASYNC (Déclaration) ---
    case NODE_ASYNC: {
        // Dans notre interpréteur synchrone, on traite 'async func' comme une func normale
        // Mais on pourrait marquer un flag "is_async" dans la structure Function
        if (node->left && node->left->type == NODE_FUNC) {
            // On délègue l'enregistrement au NODE_FUNC standard
            execute(node->left); 
            
            // Optionnel : Retrouver la fonction et marquer comme async
            // Optionnel : Retrouver la fonction et marquer comme async
            if (node->left->data.name) {
                // Function* f = findFunction(node->left->data.name); // Variable supprimée pour éviter le warning
                
            }
        }
        break;
    }         
       
        case NODE_FUNC: {
    // Enregistrer la fonction SEULEMENT si on est dans le scope global
    // et si ce n'est pas dans un module importé (géré par loadAndExecuteModule)
    if (node->data.name && scope_level == 0) {
                               
        int param_count = 0;
        ASTNode* param = node->left;
        while (param) {
            param_count++;
            param = param->right;
        }
        
        registerFunction(node->data.name, node->left, node->right, param_count);
    }
    break;
} 
            


    case NODE_FUNC_CALL: {
        char* func_name = node->data.name;
        char* prev_this = current_this; // Sauvegarde du contexte
        
        // --- [OOP FIX] LOGIQUE APPEL METHODE (void) ---
        char* dot = strchr(func_name, '.');
        char real_func_name[256];
        bool is_method = false;

        if (dot) {
            // Parsing "app.install" -> var="app", method="install"
            int len = dot - func_name;
            char var_name[128];
            strncpy(var_name, func_name, len);
            var_name[len] = '\0';
            char* method = dot + 1;
            
            // 1. Trouver l'instance dans la variable
            int idx = findVar(var_name);
            if (idx >= 0 && vars[idx].is_string) {
                char* instance_id = vars[idx].value.str_val; // ex: "inst_1"
                
                // 2. Trouver la classe de l'instance
                char* cls = findClassOf(instance_id); // ex: "Zarch"
                if (cls) {
                    // 3. Rediriger vers "Zarch_install"
                    snprintf(real_func_name, 256, "%s_%s", cls, method);
                    func_name = real_func_name;
                    
                    // 4. Injecter 'this'
                    current_this = instance_id;
                    is_method = true;
                }
            }
        }

        Function* func = findFunction(func_name);
        
        if (func) {
            Function* prev_func = current_function;
            current_function = func;
            
            int old_scope = scope_level;
            scope_level++;
            
            // --- PASSAGE ARGUMENTS ---
            if (node->left && func->param_names) {
                ASTNode* arg = node->left;
                int param_idx = 0;
                
                while (arg && param_idx < func->param_count) {
                    if (func->param_names[param_idx]) {
                        if (var_count < 1000) {
                            Variable* var = &vars[var_count];
                            strncpy(var->name, func->param_names[param_idx], 99);
                            var->name[99] = '\0';
                            var->type = TK_VAR;
                            var->scope_level = scope_level;
                            var->is_constant = false;
                            var->is_initialized = true;
                            var->is_locked = false; // Par défaut non verrouillé
                            
                            // Évaluation de l'argument
                            if (arg->type == NODE_STRING) {
                                var->is_string = true;
                                var->is_float = false;
                                var->value.str_val = evalString(arg);
                            } else {
                                var->is_float = true;
                                var->is_string = false;
                                var->value.float_val = evalFloat(arg);
                            }
                            var_count++;
                        }
                    }
                    arg = arg->right;
                    param_idx++;
                }
            }
            
            // --- EXECUTION ---
            func->has_returned = false;
            if (func->body) execute(func->body);
            
            // --- NETTOYAGE ---
            scope_level = old_scope;
            current_function = prev_func;
            
            // Restauration du this pour les appels imbriqués
            if (is_method) current_this = prev_this;
            
        } else {
            printf(node, "function not exec '%s'", func_name);
        }
        
        // Sécurité en cas d'erreur
        if (is_method && current_this != prev_this) current_this = prev_this;
        break;
    }
            
       case NODE_LOCK: {
        // Syntaxe: lock(variable) { code... }
        if (node->left && node->left->type == NODE_IDENT) {
            int idx = findVar(node->left->data.name);
            if (idx >= 0) {
                if (vars[idx].is_locked) {
                    printf("Cannot not lock ",);
                    return;
                }
                // Verrouillage
                vars[idx].is_locked = true;
                
                // Exécution du bloc sécurisé
                execute(node->right);
                
                // Déverrouillage
                vars[idx].is_locked = false;
            }
        }
        break;
    } 
            
        case NODE_TYPEDEF:
            break;
            
        case NODE_JSON:
            break;
            
        case NODE_BINARY:
        case NODE_UNARY:
        case NODE_TERNARY:
            evalFloat(node);
            break;
            
        case NODE_LIST:
        case NODE_MAP:
            evalFloat(node);
            break;
            
        default:
            break;
    }
}

// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void run(const char* source, const char* filename) {
    initWorkingDir(filename);
    
        
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (!nodes) {
        runtime_error(node, "Cannot run '%s'", filename);
        return;
    }
    
    // 1. ÉTAPE DE PRÉ-ENREGISTREMENT (Fonctions et Classes)
    for (int i = 0; i < count; i++) {
        if (nodes[i]) {
            if (nodes[i]->type == NODE_FUNC) {
                int param_count = 0;
                ASTNode* param = nodes[i]->left;
                while (param) {
                    param_count++;
                    param = param->right;
                }
                registerFunction(nodes[i]->data.name, nodes[i]->left, nodes[i]->right, param_count);
            } else if (nodes[i]->type == NODE_CLASS) {
                execute(nodes[i]); // Enregistrement des classes
            }
        }
    }
    
    ASTNode* main_node = NULL;
    
    // 2. ÉTAPE D'EXÉCUTION GLOBALE (Imports, Variables Globales, etc.)
    // On exécute tout ce qui n'est PAS une définition de fonction, ni le main
    for (int i = 0; i < count; i++) {
        if (!nodes[i]) continue;

        // On sauvegarde le pointeur vers main pour plus tard
        if (nodes[i]->type == NODE_MAIN) {
            main_node = nodes[i];
            continue; 
        }

        // On ignore les définitions de fonctions (déjà enregistrées à l'étape 1)
        if (nodes[i]->type == NODE_FUNC || nodes[i]->type == NODE_CLASS) {
            continue;
        }

        // On exécute tout le reste (Imports, Variables globales, print, etc.)
        execute(nodes[i]);
    }
    
    // 3. ÉTAPE D'EXÉCUTION DU MAIN
    if (main_node) {
        execute(main_node);
    }
    
    // NETTOYAGE
    for (int i = 0; i < count; i++) {
        if (nodes[i]) {
            if (nodes[i]->type == NODE_STRING && nodes[i]->data.str_val) {
                free(nodes[i]->data.str_val);
            }
            if (nodes[i]->type == NODE_IDENT && nodes[i]->data.name) {
                free(nodes[i]->data.name);
            }
            free(nodes[i]);
        }
    }
    free(nodes);
    
    // Nettoyage variables globales
    for (int i = 0; i < var_count; i++) {
        if (vars[i].is_string && vars[i].value.str_val) {
            free(vars[i].value.str_val);
        }
    }
    var_count = 0;
    scope_level = 0;
    
    // Nettoyage fonctions
    for (int i = 0; i < func_count; i++) {
        if (functions[i].param_names) {
            for (int j = 0; j < functions[i].param_count; j++) {
                free(functions[i].param_names[j]);
            }
            free(functions[i].param_names);
        }
        if (functions[i].return_string) {
            free(functions[i].return_string);
        }
    }
    func_count = 0;
    current_function = NULL;
    
    for (int i = 0; i < class_count; i++) {
        if (classes[i].parent) free(classes[i].parent);
    }
    class_count = 0;
}

// ======================================================
// [SECTION] REPL
// ======================================================
static void repl() {
    printf("\n");
    printf("%s╔════════════════════════════════════════════════════════════════╗%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║              SWIFT FLOW INTERACTIVE REPL                      ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║  Type 'exit' to quit, 'clear' to clear screen, 'dbvar' for    ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s║  variable information, or any SwiftFlow code to execute.      ║%s\n", COLOR_CYAN, COLOR_RESET);
    printf("%s╚════════════════════════════════════════════════════════════════╝%s\n", COLOR_CYAN, COLOR_RESET);
    printf("\n");
    
    char line[4096];
    while (1) {
        printf("%sswift>>%s ", COLOR_BRIGHT_GREEN, COLOR_RESET);
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0) break;
        if (strcmp(line, "quit") == 0) break;
        if (strcmp(line, "clear") == 0) {
            system("clear");
            continue;
        }
        if (strcmp(line, "cls") == 0) {
            system("clear");
            continue;
        }
        if (strcmp(line, "dbvar") == 0) {
            ASTNode node;
            memset(&node, 0, sizeof(node));
            node.type = NODE_DBVAR;
            execute(&node);
            continue;
        }
        if (strcmp(line, "version") == 0) {
            showVersion();
            continue;
        }
        if (strcmp(line, "help") == 0) {
            showHelp();
            continue;
        }
        if (strlen(line) == 0) continue;
        
        run(line, "REPL");
    }
    
    printf("\n%s[REPL]%s Goodbye! Thanks for using SwiftFlow.\n", COLOR_BLUE, COLOR_RESET);
}

// ======================================================
// [SECTION] FILE LOADING
// ======================================================
static char* loadFile(const char* filename) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        runtime_error(node, "Cannot assign to constant '%s'", target_name);
        return NULL;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        fclose(f);
        return NULL;
    }
    
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    return source;
}

// ======================================================
// [SECTION] MAIN
// ======================================================
int main(int argc, char* argv[]) {
    srand(time(NULL));
    init_io_module();
    init_sys_module(argc, argv);
    init_http_module();
    // Handle command line arguments
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            showVersion();
            return 0;
        }
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            showHelp();
            return 0;
        }
    }
    
    if (argc < 2) {
        // No filename provided, start REPL
        repl();
    } else {
        // Check if first argument is a flag
        if (argv[1][0] == '-') {
            // It's a flag but not recognized, show error
            runtime_error(node, "Cannot assign to constant '%s'", target_name);
            printf("Use %s--help%s for usage information.\n", COLOR_CYAN, COLOR_RESET);
            return 1;
        }
        
        // Load and run the file
        char* source = loadFile(argv[1]);
        if (!source) {
            return 1;
        }
        
        run(source, argv[1]);
        free(source);
    }
    
    return 0;
}

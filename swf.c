#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
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
#include <sqlite3.h>
#include <time.h>

#define USE_SQLITE
// ======================================================
// [SECTION] IMPORT DATABASE GLOBALS
// ======================================================
static sqlite3* import_db = NULL;
static bool db_initialized = false;
static char* db_path = "swift_imports.db";
// ======================================================
// [SECTION] GLOBAL STATE
// ======================================================
static char current_working_dir[PATH_MAX];
extern ASTNode** parse(const char* source, int* count);

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
// [SECTION] IMPORT SYSTEM
// ======================================================
typedef struct {
    char* name;
    char* file_path;
    bool is_loaded;
} ImportedModule;

static ImportedModule imports[100];
static int import_count = 0;

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

static bool isLocalImport(const char* import_path) {
    return import_path[0] == '.' || import_path[0] == '/';
}
static char* resolveModulePath(const char* import_path, const char* from_module) {
    char* resolved = malloc(PATH_MAX);
    if (!resolved) return NULL;
    
    // Si chemin absolu
    if (import_path[0] == '/') {
        snprintf(resolved, PATH_MAX, "%s", import_path);
        if (!strstr(resolved, ".swf")) {
            strcat(resolved, ".swf");
        }
        return resolved;
    }
    
    // Si chemin relatif (commence par .)
    if (import_path[0] == '.') {
        if (from_module && from_module[0]) {
            // Relatif au module appelant
            char module_dir[PATH_MAX];
            strncpy(module_dir, from_module, PATH_MAX);
            char* dir = dirname(module_dir);
            snprintf(resolved, PATH_MAX, "%s/%s", dir, import_path);
        } else {
            // Relatif au répertoire courant
            snprintf(resolved, PATH_MAX, "%s/%s", current_working_dir, import_path);
        }
        
        // Normaliser le chemin
        char normalized[PATH_MAX];
        if (realpath(resolved, normalized) != NULL) {  // CORRIGÉ: != NULL au lieu de ) != NULL)
            free(resolved);
            resolved = strdup(normalized);
        }
        
        if (!strstr(resolved, ".swf")) {
            strcat(resolved, ".swf");
        }
        return resolved;
    }
    
    // Module sans chemin (chercher dans les chemins système)
    const char* search_paths[] = {
        current_working_dir,
        "/usr/local/lib/swift/modules/",
        "/usr/local/lib/swift/packages/",
        "/usr/local/lib/swift/lib/",
        NULL
    };
    
    for (int i = 0; search_paths[i]; i++) {
        snprintf(resolved, PATH_MAX, "%s/%s.swf", search_paths[i], import_path);
        if (access(resolved, F_OK) == 0) {
            return resolved;
        }
        
        snprintf(resolved, PATH_MAX, "%s/%s/%s.swf", search_paths[i], import_path, import_path);
        if (access(resolved, F_OK) == 0) {
            return resolved;
        }
    }
    
    free(resolved);
    return NULL;
}
// ======================================================
// [SECTION] IMPORT DATABASE FUNCTIONS
// ======================================================

static bool init_import_db() {
    if (db_initialized && import_db) return true;
    
    int rc = sqlite3_open(db_path, &import_db);
    if (rc != SQLITE_OK) {
        printf("%s[DB ERROR]%s Cannot open database: %s\n", 
               COLOR_RED, COLOR_RESET, sqlite3_errmsg(import_db));
        import_db = NULL;
        return false;
    }
    
    // Créer les tables si elles n'existent pas
    const char* create_tables = 
        "CREATE TABLE IF NOT EXISTS imports ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "module_path TEXT NOT NULL,"
        "resolved_path TEXT NOT NULL,"
        "from_module TEXT,"
        "import_time INTEGER NOT NULL,"
        "UNIQUE(module_path, from_module)"
        ");"
        "CREATE TABLE IF NOT EXISTS exports ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "module_path TEXT NOT NULL,"
        "symbol TEXT NOT NULL,"
        "alias TEXT,"
        "symbol_type TEXT NOT NULL,"  // 'function', 'constant', 'variable', etc.
        "import_id INTEGER,"
        "FOREIGN KEY(import_id) REFERENCES imports(id),"
        "UNIQUE(module_path, symbol)"
        ");"
        "CREATE INDEX IF NOT EXISTS idx_imports_path ON imports(module_path);"
        "CREATE INDEX IF NOT EXISTS idx_exports_module ON exports(module_path);"
        "CREATE INDEX IF NOT EXISTS idx_exports_symbol ON exports(symbol);";
    
    char* err_msg = NULL;
    rc = sqlite3_exec(import_db, create_tables, NULL, NULL, &err_msg);
    
    if (rc != SQLITE_OK) {
        printf("%s[DB ERROR]%s Cannot create tables: %s\n", 
               COLOR_RED, COLOR_RESET, err_msg);
        sqlite3_free(err_msg);
        sqlite3_close(import_db);
        import_db = NULL;
        return false;
    }
    
    db_initialized = true;
    printf("%s[DB]%s Import database initialized: %s\n", 
           COLOR_GREEN, COLOR_RESET, db_path);
    return true;
}

static void close_import_db() {
    if (import_db) {
        sqlite3_close(import_db);
        import_db = NULL;
    }
    db_initialized = false;
    printf("%s[DB]%s Import database closed\n", COLOR_GREEN, COLOR_RESET);
}

static bool register_import(const char* module_path, const char* resolved_path, 
                           const char* from_module) {
    if (!init_import_db()) return false;
    
    // Vérifier si déjà importé
    sqlite3_stmt* stmt;
    const char* check_sql = "SELECT id FROM imports WHERE module_path = ? AND from_module = ?";
    
    int rc = sqlite3_prepare_v2(import_db, check_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, module_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, from_module ? from_module : "", -1, SQLITE_STATIC);
    
    bool already_imported = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    
    if (already_imported) {
        printf("%s[DB INFO]%s Module already imported: %s\n", 
               COLOR_YELLOW, COLOR_RESET, module_path);
        return true;
    }
    
    // Insérer le nouvel import
    const char* insert_sql = 
        "INSERT INTO imports (module_path, resolved_path, from_module, import_time) "
        "VALUES (?, ?, ?, ?)";
    
    rc = sqlite3_prepare_v2(import_db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, module_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, resolved_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, from_module ? from_module : "", -1, SQLITE_STATIC);
    sqlite3_bind_int64(stmt, 4, time(NULL));
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        printf("%s[DB ERROR]%s Failed to register import: %s\n", 
               COLOR_RED, COLOR_RESET, module_path);
        return false;
    }
    
    printf("%s[DB]%s Registered import: %s\n", COLOR_GREEN, COLOR_RESET, module_path);
    return true;
}

static bool record_export(const char* module_path, const char* symbol, 
                         const char* alias, const char* symbol_type) {
    if (!init_import_db()) return false;
    
    // Obtenir l'ID de l'import
    sqlite3_stmt* stmt;
    const char* get_import_sql = 
        "SELECT id FROM imports WHERE module_path = ? ORDER BY import_time DESC LIMIT 1";
    
    int rc = sqlite3_prepare_v2(import_db, get_import_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, module_path, -1, SQLITE_STATIC);
    
    int import_id = -1;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        import_id = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    
    if (import_id == -1) {
        printf("%s[DB WARNING]%s No import record found for: %s\n", 
               COLOR_YELLOW, COLOR_RESET, module_path);
        return false;
    }
    
    // Insérer l'export
    const char* insert_sql = 
        "INSERT OR REPLACE INTO exports (module_path, symbol, alias, symbol_type, import_id) "
        "VALUES (?, ?, ?, ?, ?)";
    
    rc = sqlite3_prepare_v2(import_db, insert_sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, module_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, symbol, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 3, alias ? alias : symbol, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 4, symbol_type, -1, SQLITE_STATIC);
    sqlite3_bind_int(stmt, 5, import_id);
    
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    
    if (rc != SQLITE_DONE) {
        printf("%s[DB ERROR]%s Failed to record export: %s.%s\n", 
               COLOR_RED, COLOR_RESET, module_path, symbol);
        return false;
    }
    
    printf("%s[DB]%s Recorded export: %s.%s as %s\n", 
           COLOR_GREEN, COLOR_RESET, module_path, symbol, alias ? alias : symbol);
    return true;
}

static bool is_module_imported(const char* module_path, const char* from_module) {
    if (!init_import_db()) return false;
    
    sqlite3_stmt* stmt;
    const char* sql = "SELECT 1 FROM imports WHERE module_path = ? AND from_module = ?";
    
    int rc = sqlite3_prepare_v2(import_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return false;
    
    sqlite3_bind_text(stmt, 1, module_path, -1, SQLITE_STATIC);
    sqlite3_bind_text(stmt, 2, from_module ? from_module : "", -1, SQLITE_STATIC);
    
    bool imported = (sqlite3_step(stmt) == SQLITE_ROW);
    sqlite3_finalize(stmt);
    
    return imported;
}

static void show_import_db() {
    if (!init_import_db()) {
        printf("%s[DB ERROR]%s Database not initialized\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    printf("\n%s╔════════════════════════════════════════════════════════════════╗%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s║                    IMPORT DATABASE (importdb)                  ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", 
           COLOR_CYAN, COLOR_RESET);
    
    // Afficher les imports
    printf("%s║                           IMPORTS                               ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", 
           COLOR_CYAN, COLOR_RESET);
    
    sqlite3_stmt* stmt;
    const char* import_sql = 
        "SELECT module_path, resolved_path, from_module, "
        "datetime(import_time, 'unixepoch') as import_time "
        "FROM imports ORDER BY import_time DESC";
    
    int rc = sqlite3_prepare_v2(import_db, import_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            count++;
            const char* module = (const char*)sqlite3_column_text(stmt, 0);
            const char* resolved = (const char*)sqlite3_column_text(stmt, 1);
            const char* from = (const char*)sqlite3_column_text(stmt, 2);
            const char* import_time = (const char*)sqlite3_column_text(stmt, 3);
            
            printf("%s║ %-30s → %-30s ║%s\n", COLOR_CYAN, module, resolved, COLOR_RESET);
            printf("%s║   From: %-20s  Time: %-19s ║%s\n", 
                   COLOR_CYAN, from[0] ? from : "(main)", import_time, COLOR_RESET);
            printf("%s║                                                          ║%s\n",
                   COLOR_CYAN, COLOR_RESET);
        }
        
        if (count == 0) {
            printf("%s║                   No imports recorded                     ║%s\n", 
                   COLOR_CYAN, COLOR_RESET);
        }
        sqlite3_finalize(stmt);
    }
    
    // Afficher les exports
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s║                           EXPORTS                               ║%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("%s╠════════════════════════════════════════════════════════════════╣%s\n", 
           COLOR_CYAN, COLOR_RESET);
    
    const char* export_sql = 
        "SELECT e.module_path, e.symbol, e.alias, e.symbol_type, "
        "datetime(i.import_time, 'unixepoch') as import_time "
        "FROM exports e LEFT JOIN imports i ON e.import_id = i.id "
        "ORDER BY e.module_path, e.symbol";
    
    rc = sqlite3_prepare_v2(import_db, export_sql, -1, &stmt, NULL);
    if (rc == SQLITE_OK) {
        int count = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW) {
            count++;
            const char* module = (const char*)sqlite3_column_text(stmt, 0);
            const char* symbol = (const char*)sqlite3_column_text(stmt, 1);
            const char* alias = (const char*)sqlite3_column_text(stmt, 2);
            const char* type = (const char*)sqlite3_column_text(stmt, 3);
            const char* import_time = (const char*)sqlite3_column_text(stmt, 4);
            
            const char* color = COLOR_CYAN;
            if (strcmp(type, "function") == 0) color = COLOR_GREEN;
            else if (strcmp(type, "constant") == 0) color = COLOR_YELLOW;
            else if (strcmp(type, "variable") == 0) color = COLOR_BLUE;
            
            printf("%s║ %s%-10s%s %-15s → %-15s (from %-20s) ║%s\n", 
                   COLOR_CYAN, color, type, COLOR_CYAN, 
                   symbol, alias, module, COLOR_RESET);
        }
        
        if (count == 0) {
            printf("%s║          No exports record  ║%s\n", 
                   COLOR_CYAN, COLOR_RESET);
        }
        sqlite3_finalize(stmt);
    }
    
    printf("%s╚══════════════════════════════════════════════╝%s\n", 
           COLOR_CYAN, COLOR_RESET);
    printf("\n");
}

static void clear_import_db() {
    if (!init_import_db()) return;
    
    const char* clear_sql = "DELETE FROM imports; DELETE FROM exports;";
    char* err_msg = NULL;
    
    int rc = sqlite3_exec(import_db, clear_sql, NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        printf("%s[DB ERROR]%s Failed to clear database: %s\n", 
               COLOR_RED, COLOR_RESET, err_msg);
        sqlite3_free(err_msg);
    } else {
        printf("%s[DB]%s Import database cleared\n", COLOR_GREEN, COLOR_RESET);
    }
}
// ======================================================
// [SECTION] FONCTION D'IMPORT - MODIFIÉE
// ======================================================
static bool loadAndExecuteModule(const char* import_path, const char* from_module, 
                                 bool import_named, char** named_symbols, int symbol_count) {
    printf("\n%s-%s\n", COLOR_BRIGHT_RED, COLOR_RESET);
    printf("%s>>> LOAD AND EXEC MODULE: %s <<<%s\n", COLOR_BRIGHT_RED, COLOR_RESET, import_path);
    printf("%s-%s\n\n", COLOR_BRIGHT_RED, COLOR_RESET);
    
    // 1. Résoudre le chemin
    char* full_path = resolveModulePath(import_path, from_module);
    if (!full_path) {
        printf("%s[IMPORT ERROR]%s Cannot resolve module path: %s\n", COLOR_RED, COLOR_RESET, import_path);
        return false;
    }
    
    printf("%s[IMPORT DEBUG]%s Resolved path: %s\n", COLOR_YELLOW, COLOR_RESET, full_path);
    
    // 2. Vérifier si déjà importé (version simple)
    bool already_imported = false;
    for (int i = 0; i < import_count; i++) {
        if (strcmp(import_db[i].module_path, import_path) == 0 &&
            strcmp(import_db[i].from_module, from_module ? from_module : "") == 0) {
            already_imported = true;
            break;
        }
    }
    
    if (already_imported) {
        printf("%s[IMPORT INFO]%s Module already imported: %s\n", COLOR_YELLOW, COLOR_RESET, import_path);
        free(full_path);
        return true;
    }
    
    // 3. Ouvrir le fichier
    FILE* f = fopen(full_path, "r");
    if (!f) {
        printf("%s[IMPORT ERROR]%s Cannot open file: %s\n", COLOR_RED, COLOR_RESET, full_path);
        free(full_path);
        return false;
    }
    
    // 4. Lire le fichier
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    if (!source) {
        printf("%s[IMPORT ERROR]%s Memory allocation failed\n", COLOR_RED, COLOR_RESET);
        fclose(f);
        free(full_path);
        return false;
    }
    
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    printf("%s[IMPORT DEBUG]%s File read: %ld bytes\n", COLOR_YELLOW, COLOR_RESET, size);
    
    // 5. Sauvegarder l'état
    char old_dir[PATH_MAX];
    strncpy(old_dir, current_working_dir, PATH_MAX);
    
    char module_dir[PATH_MAX];
    strncpy(module_dir, full_path, PATH_MAX);
    char* dir = dirname(module_dir);
    strncpy(current_working_dir, dir, PATH_MAX);
    
    printf("%s[IMPORT DEBUG]%s Working directory changed: %s -> %s\n", COLOR_YELLOW, COLOR_RESET, old_dir, current_working_dir);
    
    // 6. Sauvegarder les compteurs avant
    int old_export_count = export_count;
    int old_func_count = func_count;
    int old_var_count = var_count;
    
    // 7. Parser le module
    printf("%s[IMPORT DEBUG]%s Parsing module...\n", COLOR_YELLOW, COLOR_RESET);
    int node_count = 0;
    ASTNode** nodes = parse(source, &node_count);
    
    if (!nodes) {
        printf("%s[IMPORT ERROR]%s Parsing failed\n", COLOR_RED, COLOR_RESET);
        free(source);
        free(full_path);
        strncpy(current_working_dir, old_dir, PATH_MAX);
        return false;
    }
    
    printf("%s[IMPORT DEBUG]%s Module parsed successfully: %d nodes\n", COLOR_GREEN, COLOR_RESET, node_count);
    
    // === CRITICAL FIX: Register functions from parsed nodes ===
    printf("%s[CRITICAL FIX]%s Searching for functions in %d nodes...\n", COLOR_BRIGHT_RED, COLOR_RESET, node_count);
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i] && nodes[i]->type == NODE_FUNC && nodes[i]->data.name) {
            printf("%s[CRITICAL FIX]%s Found function: %s\n", COLOR_BRIGHT_RED, COLOR_RESET, nodes[i]->data.name);
            
            // Compter les paramètres
            int param_count = 0;
            ASTNode* param = nodes[i]->left;
            while (param) {
                param_count++;
                param = param->right;
            }
            
            // Enregistrer la fonction directement
            registerFunction(nodes[i]->data.name, nodes[i]->left, nodes[i]->right, param_count);
            printf("%s[CRITICAL FIX]%s Registered function: %s (%d params)\n", COLOR_BRIGHT_GREEN, COLOR_RESET, nodes[i]->data.name, param_count);
        }
        
        // Aussi chercher les constantes
        if (nodes[i] && (nodes[i]->type == NODE_CONST_DECL || nodes[i]->type == NODE_VAR_DECL) && nodes[i]->data.name) {
            printf("%s[CRITICAL FIX]%s Found variable: %s\n", COLOR_BRIGHT_RED, COLOR_RESET, nodes[i]->data.name);
            
            // Créer la variable directement
            if (var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, nodes[i]->data.name, 99);
                var->name[99] = '\0';
                var->type = (nodes[i]->type == NODE_CONST_DECL) ? TK_CONST : TK_VAR;
                var->scope_level = 0;
                var->is_constant = (nodes[i]->type == NODE_CONST_DECL);
                var->is_initialized = true;
                
                if (nodes[i]->left) {
                    if (nodes[i]->left->type == NODE_INT) {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = nodes[i]->left->data.int_val;
                    } else if (nodes[i]->left->type == NODE_FLOAT) {
                        var->is_float = true;
                        var->is_string = false;
                        var->value.float_val = nodes[i]->left->data.float_val;
                    } else if (nodes[i]->left->type == NODE_STRING) {
                        var->is_string = true;
                        var->is_float = false;
                        var->value.str_val = str_copy(nodes[i]->left->data.str_val);
                    }
                }
                
                var_count++;
                printf("%s[CRITICAL FIX]%s Created variable: %s\n", COLOR_BRIGHT_GREEN, COLOR_RESET, nodes[i]->data.name);
            }
        }
    }
    
    printf("%s[CRITICAL FIX]%s After direct registration: func_count=%d, var_count=%d\n", COLOR_BRIGHT_RED, COLOR_RESET, func_count, var_count);
    
    // 8. Exécuter les déclarations pour enregistrer les exports
    printf("%s[IMPORT DEBUG]%s Executing declarations to register exports...\n", COLOR_YELLOW, COLOR_RESET);
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i] && (nodes[i]->type == NODE_EXPORT || nodes[i]->type == NODE_FUNC || 
                         nodes[i]->type == NODE_CONST_DECL || nodes[i]->type == NODE_VAR_DECL)) {
            printf("%s[IMPORT DEBUG]%s Executing node %d (type: %d)\n", COLOR_YELLOW, COLOR_RESET, i, nodes[i]->type);
            execute(nodes[i]);
        }
    }
    
    // 9. Enregistrer dans la DB simple
    if (import_count < 1000) {
        ImportRecord* rec = &import_db[import_count];
        rec->module_path = str_copy(import_path);
        rec->resolved_path = str_copy(full_path);
        rec->from_module = str_copy(from_module ? from_module : "");
        rec->import_time = time(NULL);
        rec->active = true;
        import_count++;
        printf("%s[DB]%s Registered import: %s\n", COLOR_GREEN, COLOR_RESET, import_path);
    }
    
    // 10. Enregistrer les exports dans la DB simple
    for (int i = old_export_count; i < export_count; i++) {
        ExportEntry* exp = &exports[i];
        
        // Déterminer le type
        const char* type = "unknown";
        for (int j = 0; j < node_count; j++) {
            if (nodes[j] && nodes[j]->data.name && strcmp(nodes[j]->data.name, exp->symbol) == 0) {
                if (nodes[j]->type == NODE_FUNC) type = "function";
                else if (nodes[j]->type == NODE_CONST_DECL) type = "constant";
                else if (nodes[j]->type == NODE_VAR_DECL) type = "variable";
                break;
            }
        }
        
        // Enregistrer
        if (export_count < 5000) {
            ExportRecord* erec = &export_db[export_count];
            erec->module_path = str_copy(import_path);
            erec->symbol = str_copy(exp->symbol);
            erec->alias = str_copy(exp->alias ? exp->alias : exp->symbol);
            erec->symbol_type = str_copy(type);
            erec->import_id = import_count - 1;
            export_count++;
            printf("%s[DB]%s Recorded export: %s.%s as %s\n", COLOR_GREEN, COLOR_RESET, 
                   import_path, exp->symbol, exp->alias ? exp->alias : exp->symbol);
        }
    }
    
    // 11. Exécuter le reste du code
    printf("%s[IMPORT DEBUG]%s Executing module initialization code...\n", COLOR_YELLOW, COLOR_RESET);
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i] && nodes[i]->type != NODE_EXPORT && nodes[i]->type != NODE_FUNC && 
            nodes[i]->type != NODE_CONST_DECL && nodes[i]->type != NODE_VAR_DECL) {
            printf("%s[IMPORT DEBUG]%s Executing node %d (type: %d)\n", COLOR_YELLOW, COLOR_RESET, i, nodes[i]->type);
            execute(nodes[i]);
        }
    }
    
    // 12. Restaurer l'état
    strncpy(current_working_dir, old_dir, PATH_MAX);
    printf("%s[IMPORT DEBUG]%s Working directory restored: %s\n", COLOR_YELLOW, COLOR_RESET, current_working_dir);
    
    // 13. Nettoyer
    printf("%s[IMPORT DEBUG]%s Cleaning up AST nodes...\n", COLOR_YELLOW, COLOR_RESET);
    
    for (int i = 0; i < node_count; i++) {
        if (nodes[i]) {
            if (nodes[i]->type == NODE_STRING && nodes[i]->data.str_val) free(nodes[i]->data.str_val);
            if (nodes[i]->type == NODE_IDENT && nodes[i]->data.name) free(nodes[i]->data.name);
            if (nodes[i]->type == NODE_FUNC && nodes[i]->data.name) free(nodes[i]->data.name);
            if (nodes[i]->type == NODE_EXPORT) {
                if (nodes[i]->data.export.symbol) free(nodes[i]->data.export.symbol);
                if (nodes[i]->data.export.alias) free(nodes[i]->data.export.alias);
            }
            free(nodes[i]);
        }
    }
    free(nodes);
    free(source);
    free(full_path);
    
    printf("%s[IMPORT]%s Module import completed: %s\n", COLOR_GREEN, COLOR_RESET, import_path);
    printf("%s[IMPORT DEBUG]%s Functions registered: %d (new: %d)\n", COLOR_GREEN, COLOR_RESET, 
           func_count, func_count - old_func_count);
    printf("%s[IMPORT DEBUG]%s Variables: %d (new: %d)\n", COLOR_GREEN, COLOR_RESET, 
           var_count, var_count - old_var_count);
    printf("%s[IMPORT DEBUG]%s Exports: %d (new: %d)\n", COLOR_GREEN, COLOR_RESET, 
           export_count, export_count - old_export_count);
    
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
// Enregistrer dans la base de données
    register_import(import_path, full_path, from_module);
    
    // Enregistrer tous les exports dans la DB
    for (int i = old_export_count; i < export_count; i++) {
        ExportEntry* exp = &exports[i];
        if (exp->symbol && strstr(exp->module, import_path)) {
            // Déterminer le type
            const char* type = "unknown";
            for (int j = 0; j < node_count; j++) {
                if (nodes[j] && nodes[j]->data.name && 
                    strcmp(nodes[j]->data.name, exp->symbol) == 0) {
                    if (nodes[j]->type == NODE_FUNC) type = "function";
                    else if (nodes[j]->type == NODE_CONST_DECL) type = "constant";
                    else if (nodes[j]->type == NODE_VAR_DECL) type = "variable";
                    break;
                }
            }
            
            record_export(import_path, exp->symbol, exp->alias, type);
        }
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
                if (isnan(val)) {
                    strcpy(result, "nan");
                } else if (isinf(val)) {
                    strcpy(result, val > 0 ? "inf" : "-inf");
                } else if (fabs(val - (int64_t)val) < 1e-10) {
                    sprintf(result, "%lld", (int64_t)val);
                } else {
                    sprintf(result, "%g", val);
                }
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
                        if (isnan(val)) {
                            strcpy(result, "nan");
                        } else if (isinf(val)) {
                            strcpy(result, val > 0 ? "inf" : "-inf");
                        } else if (fabs(val - (int64_t)val) < 1e-10) {
                            sprintf(result, "%lld", (int64_t)val);
                        } else {
                            sprintf(result, "%g", val);
                        }
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
                if (result) {
                    if (isnan(val)) {
                        strcpy(result, "nan");
                    } else if (isinf(val)) {
                        strcpy(result, val > 0 ? "inf" : "-inf");
                    } else if (fabs(val - (int64_t)val) < 1e-10) {
                        sprintf(result, "%lld", (int64_t)val);
                    } else {
                        sprintf(result, "%g", val);
                    }
                }
                return result ? result : str_copy("");
            }
        }
            
        case NODE_FUNC_CALL: {
            Function* func = findFunction(node->data.name);
            if (func) {
                evalFloat(node);
                
                if (func->return_string) {
                    return str_copy(func->return_string);
                } else {
                    char* result = malloc(32);
                    if (result) {
                        sprintf(result, "%g", func->return_value);
                    }
                    return result ? result : str_copy("");
                }
            } else {
                return str_copy("undefined");
            }
        }
            
        default:
            return str_copy("");
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

// ======================================================
// [SECTION] MAIN EXECUTION FUNCTION
// ======================================================
static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_EXPORT: {
    printf("%s[EXECUTE EXPORT]%s Processing export\n", COLOR_MAGENTA, COLOR_RESET);
    
    if (node->data.export.symbol) {
        char* symbol = node->data.export.symbol;
        char* alias = node->data.export.alias ? node->data.export.alias : symbol;
        
        printf("%s[EXECUTE EXPORT]%s Registering export: %s as %s\n",
               COLOR_MAGENTA, COLOR_RESET, symbol, alias);
        
        registerExport(symbol, alias);
    }
    break;
}
        case NODE_IMPORTDB:
    show_import_db();
    break;
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
            
            if (var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, node->data.name, 99);
                var->name[99] = '\0';
                var->type = var_type;
                var->size_bytes = calculateVariableSize(var_type);
                var->scope_level = scope_level;
                var->is_constant = (var_type == TK_CONST);
                var->module = NULL;
                var->is_exported = false;
                
                if (node->left) {
                    var->is_initialized = true;
                    
                    if (node->left->type == NODE_STRING) {
                        var->is_string = true;
                        var->is_float = false;
                        var->value.str_val = str_copy(node->left->data.str_val);
                    } 
                    else if (node->left->type == NODE_FLOAT) {
                        var->is_float = true;
                        var->is_string = false;
                        var->value.float_val = evalFloat(node->left);
                    }
                    else if (node->left->type == NODE_BOOL) {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = node->left->data.bool_val ? 1 : 0;
                    }
                    else {
                        var->is_float = false;
                        var->is_string = false;
                        var->value.int_val = (int64_t)evalFloat(node->left);
                    }
                } else {
                    var->is_initialized = false;
                    var->is_float = false;
                    var->is_string = false;
                    var->value.int_val = 0;
                }
                
                var_count++;
                printf("%s[EXEC]%s Variable '%s' declared\n", COLOR_CYAN, COLOR_RESET, var->name);
            }
            break;
        }
        // Dans swf.c, dans la fonction execute(), après le case NODE_APPEND:
// (environ ligne 950-1000)

case NODE_FILE_OPEN:
    io_open(node);
    break;
    
case NODE_FILE_CLOSE:
    io_close(node);
    break;
    
case NODE_FILE_READ:
    io_read(node);
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
    
case NODE_PATH_EXISTS:
    io_exists(node);
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
            if (node->data.name) {
                int idx = findVar(node->data.name);
                if (idx >= 0) {
                    if (vars[idx].is_constant) {
                        printf("%s[EXEC ERROR]%s Cannot assign to constant '%s'\n", 
                               COLOR_RED, COLOR_RESET, node->data.name);
                        return;
                    }
                    
                    if (node->left) {
                        vars[idx].is_initialized = true;
                        
                        if (node->left->type == NODE_STRING) {
                            if (vars[idx].value.str_val) free(vars[idx].value.str_val);
                            vars[idx].is_string = true;
                            vars[idx].is_float = false;
                            vars[idx].value.str_val = str_copy(node->left->data.str_val);
                        }
                        else if (node->left->type == NODE_FLOAT) {
                            vars[idx].is_float = true;
                            vars[idx].is_string = false;
                            vars[idx].value.float_val = evalFloat(node->left);
                        }
                        else if (node->left->type == NODE_BOOL) {
                            vars[idx].is_float = false;
                            vars[idx].is_string = false;
                            vars[idx].value.int_val = node->left->data.bool_val ? 1 : 0;
                        }
                        else {
                            vars[idx].is_float = false;
                            vars[idx].is_string = false;
                            vars[idx].value.int_val = (int64_t)evalFloat(node->left);
                        }
                    }
                } else {
                    printf("%s[EXEC ERROR]%s Variable '%s' not found\n", 
                           COLOR_RED, COLOR_RESET, node->data.name);
                }
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
            
        case NODE_WELD: {
            char* prompt = NULL;
            if (node->left) {
                prompt = evalString(node->left);
            }
            
            char* input = weldInput(prompt);
            if (prompt) free(prompt);
            
            int idx = findVar("__weld_input__");
            if (idx == -1 && var_count < 1000) {
                Variable* var = &vars[var_count];
                strcpy(var->name, "__weld_input__");
                var->type = TK_VAR;
                var->size_bytes = strlen(input) + 1;
                var->scope_level = scope_level;
                var->is_constant = false;
                var->is_initialized = true;
                var->is_string = true;
                var->is_float = false;
                var->value.str_val = str_copy(input);
                var_count++;
            } else if (idx >= 0) {
                if (vars[idx].value.str_val) free(vars[idx].value.str_val);
                vars[idx].value.str_val = str_copy(input);
                vars[idx].is_initialized = true;
            }
            
            free(input);
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
            
        case NODE_IF: {
            bool condition = evalBool(node->left);
            if (condition) {
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
            while (evalBool(node->data.loop.condition) && !(current_function && current_function->has_returned)) {
                execute(node->data.loop.body);
                if (node->data.loop.update) execute(node->data.loop.update);
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
                    printf("%s[IMPORT ERROR]%s Failed to import named symbols from: %s\n", 
                           COLOR_RED, COLOR_RESET, module_name);
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
                    printf("%s[IMPORT ERROR]%s Failed to import: %s\n", 
                           COLOR_RED, COLOR_RESET, module_name);
                }
            }
        }
    }
    break;
}
            
       
        case NODE_FUNC: {
    // Enregistrer la fonction SEULEMENT si on est dans le scope global
    // et si ce n'est pas dans un module importé (géré par loadAndExecuteModule)
    if (node->data.name && scope_level == 0) {
        printf("%s[EXECUTE]%s Registering function: %s\n", 
               COLOR_MAGENTA, COLOR_RESET, node->data.name);
        
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
            
        case NODE_FUNC_CALL:
            evalFloat(node);
            break;
            
        case NODE_CLASS:
            registerClass(node->data.class_def.name, 
                         node->data.class_def.parent ? node->data.class_def.parent->data.name : NULL,
                         node->data.class_def.members);
            break;
        
            
        case NODE_TYPEDEF:
            printf("%s[TYPEDEF]%s Type definition\n", COLOR_CYAN, COLOR_RESET);
            break;
            
        case NODE_JSON:
            printf("%s[JSON]%s JSON data\n", COLOR_CYAN, COLOR_RESET);
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
    
    printf("%s[EXEC]%s Working directory: %s\n", COLOR_CYAN, COLOR_RESET, current_working_dir);
    
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (!nodes) {
        printf("%s[EXEC ERROR]%s Parsing failed\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
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
                execute(nodes[i]);
            }
        }
    }
    
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
        for (int i = 0; i < count; i++) {
            if (nodes[i] && nodes[i]->type != NODE_FUNC && 
                nodes[i]->type != NODE_CLASS) {
                execute(nodes[i]);
            }
        }
    }
    
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
    
    for (int i = 0; i < var_count; i++) {
        if (vars[i].is_string && vars[i].value.str_val) {
            free(vars[i].value.str_val);
        }
    }
    var_count = 0;
    scope_level = 0;
    
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
        if (strcmp(line, "importdb") == 0) {
    show_import_db();
    continue;
}
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
        printf("%s[LOAD ERROR]%s Cannot open file: %s\n", COLOR_RED, COLOR_RESET, filename);
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
    init_import_db();
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
            printf("%s[ERROR]%s Unknown option: %s\n", COLOR_RED, COLOR_RESET, argv[1]);
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
    close_import_db();
    return 0;
}

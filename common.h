#ifndef SWIFTFLOW_COMMON_H
#define SWIFTFLOW_COMMON_H

// ======================================================
// [SECTION] CONFIGURATION DE LA PLATEFORME
// ======================================================
#define _POSIX_C_SOURCE 200809L
#define SWIFTFLOW_VERSION "2.5-Fusion"
#define SWIFTFLOW_YEAR 2026

// ======================================================
// [SECTION] INCLUDES STANDARDS
// ======================================================
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <ctype.h>
#include <errno.h>
#include <math.h>

// ======================================================
// [SECTION] COULEURS POUR LE TERMINAL
// ======================================================
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define WHITE   "\033[1;37m"
#define BOLD    "\033[1m"
#define RESET   "\033[0m"

// ======================================================
// [SECTION] DÉFINITIONS DE BASE
// ======================================================
typedef enum {
    TYPE_NONE,
    TYPE_INT,
    TYPE_FLOAT,
    TYPE_STRING,
    TYPE_BOOL,
    TYPE_CHAR,
    TYPE_VOID,
    TYPE_ANY,
    TYPE_ARRAY,
    TYPE_MAP,
    TYPE_FUNCTION,
    TYPE_CLASS,
    TYPE_OBJECT,
    TYPE_ENUM,
    TYPE_TUPLE,
    TYPE_NULL,
    TYPE_UNDEFINED,
    TYPE_PROMISE,     // Pour async/await
    TYPE_GENERIC,     // Types génériques
    TYPE_UNION,       // Types union
    TYPE_INTERFACE    // Interfaces
} ValueType;

// ======================================================
// [SECTION] TOKEN DEFINITIONS (COMPLÈTE)
// ======================================================
typedef enum {
    // Literals
    TK_INT, TK_FLOAT, TK_STRING, TK_CHAR, 
    TK_TRUE, TK_FALSE, TK_NULL, TK_UNDEFINED,
    
    // Identifiers
    TK_IDENT, TK_AS, TK_IS, TK_IN,
    
    // Operators
    TK_PLUS, TK_MINUS, TK_MULT, TK_DIV, TK_MOD,
    TK_POW, TK_CONCAT, TK_SPREAD,
    TK_ASSIGN, TK_EQ, TK_NEQ, TK_GT, TK_LT, TK_GTE, TK_LTE,
    TK_AND, TK_OR, TK_NOT,
    TK_BIT_AND, TK_BIT_OR, TK_BIT_XOR, TK_BIT_NOT,
    TK_SHL, TK_SHR,
    TK_INC, TK_DEC,
    
    // Assignment operators
    TK_PLUS_ASSIGN, TK_MINUS_ASSIGN, TK_MULT_ASSIGN, 
    TK_DIV_ASSIGN, TK_MOD_ASSIGN, TK_AND_ASSIGN,
    TK_OR_ASSIGN, TK_XOR_ASSIGN, TK_SHL_ASSIGN, TK_SHR_ASSIGN,
    
    // Special operators
    TK_RARROW, TK_DARROW, TK_LDARROW, TK_RDARROW,
    TK_SPACESHIP, TK_ELLIPSIS, TK_RANGE,
    TK_QUESTION, TK_SCOPE, TK_SAFE_NAV,
    TK_AT, TK_HASH, TK_DOLLAR, TK_BACKTICK,
    
    // Punctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET, TK_LANGLE, TK_RANGLE,
    TK_COMMA, TK_SEMICOLON, TK_COLON, TK_PERIOD,
    
    // Keywords - Variables
    TK_VAR, TK_LET, TK_CONST,
    TK_NET, TK_CLOG, TK_DOS, TK_SEL,
    
    // Control flow
    TK_IF, TK_ELSE, TK_ELIF,
    TK_WHILE, TK_FOR, TK_DO, TK_FOREACH,
    TK_SWITCH, TK_CASE, TK_DEFAULT,
    TK_BREAK, TK_CONTINUE, TK_RETURN,
    TK_YIELD, TK_AWAIT,
    
    // Functions & Modules
    TK_FUNC, TK_IMPORT, TK_EXPORT, TK_FROM,
    TK_CLASS, TK_STRUCT, TK_ENUM, TK_TYPEDEF,
    TK_TYPELOCK, TK_NAMESPACE, TK_USING,
    TK_ASYNC, TK_GENERATOR,
    
    // Access modifiers
    TK_PUBLIC, TK_PRIVATE, TK_PROTECTED, TK_INTERNAL,
    TK_STATIC, TK_ABSTRACT, TK_VIRTUAL, TK_OVERRIDE,
    TK_SEALED, TK_READONLY, TK_MUTABLE,
    
    // Types
    TK_TYPE_INT, TK_TYPE_FLOAT, TK_TYPE_STR,
    TK_TYPE_BOOL, TK_TYPE_CHAR, TK_TYPE_VOID,
    TK_TYPE_ANY, TK_TYPE_AUTO, TK_TYPE_DYNAMIC,
    TK_TYPE_NET, TK_TYPE_CLOG, TK_TYPE_DOS,
    TK_TYPE_SEL, TK_TYPE_ARRAY, TK_TYPE_MAP,
    TK_TYPE_FUNC, TK_TYPE_CLASS, TK_TYPE_ENUM,
    
    // Memory & Size
    TK_SIZEOF, TK_SIZE, TK_SIZ,
    TK_NEW, TK_DELETE, TK_FREE, TK_GC,
    
    // Debug & DB
    TK_DB, TK_DBVAR, TK_PRINT_DB, TK_ASSERT,
    
    // I/O
    TK_PRINT, TK_INPUT, TK_READ, TK_WRITE,
    
    // Error handling
    TK_TRY, TK_CATCH, TK_FINALLY, TK_THROW,
    
    // Concurrency
    TK_ASYNC_FUNC, TK_AWAIT_EXPR, TK_SPAWN, TK_CHANNEL,
    TK_MUTEX, TK_LOCK, TK_UNLOCK, TK_ATOMIC,
    
    // Special
    TK_MAIN, TK_THIS, TK_SELF, TK_SUPER,
    TK_CONSTRUCTOR, TK_DESTRUCTOR,
    TK_GET, TK_SET, TK_INIT, TK_DEINIT,
    
    // Templates & Generics
    TK_TEMPLATE, TK_GENERIC, TK_WHERE,
    
    // End markers
    TK_EOF, TK_ERROR
} TokenKind;

// Structure Token
typedef struct {
    TokenKind kind;
    const char* start;
    int length;
    int line;
    int column;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        char char_val;
        bool bool_val;
    } value;
} Token;

// Table des mots-clés
typedef struct {
    const char* keyword;
    TokenKind kind;
} Keyword;

static const Keyword keywords[] = {
    // Variables
    {"var", TK_VAR}, {"let", TK_LET}, {"const", TK_CONST},
    {"net", TK_NET}, {"clog", TK_CLOG}, {"dos", TK_DOS}, {"sel", TK_SEL},
    
    // Control flow
    {"if", TK_IF}, {"else", TK_ELSE}, {"elif", TK_ELIF},
    {"while", TK_WHILE}, {"for", TK_FOR}, {"do", TK_DO}, {"foreach", TK_FOREACH},
    {"switch", TK_SWITCH}, {"case", TK_CASE}, {"default", TK_DEFAULT},
    {"break", TK_BREAK}, {"continue", TK_CONTINUE}, {"return", TK_RETURN},
    {"yield", TK_YIELD}, {"await", TK_AWAIT},
    
    // Functions
    {"func", TK_FUNC}, {"import", TK_IMPORT}, {"export", TK_EXPORT},
    {"from", TK_FROM}, {"as", TK_AS}, {"is", TK_IS}, {"in", TK_IN},
    
    // Classes & Types
    {"class", TK_CLASS}, {"struct", TK_STRUCT}, {"enum", TK_ENUM},
    {"typedef", TK_TYPEDEF}, {"typelock", TK_TYPELOCK},
    {"namespace", TK_NAMESPACE}, {"using", TK_USING},
    {"new", TK_NEW}, {"delete", TK_DELETE},
    
    // Access modifiers
    {"public", TK_PUBLIC}, {"private", TK_PRIVATE},
    {"protected", TK_PROTECTED}, {"internal", TK_INTERNAL},
    {"static", TK_STATIC}, {"abstract", TK_ABSTRACT},
    {"virtual", TK_VIRTUAL}, {"override", TK_OVERRIDE},
    {"sealed", TK_SEALED}, {"readonly", TK_READONLY},
    {"mutable", TK_MUTABLE},
    
    // Async
    {"async", TK_ASYNC}, {"await", TK_AWAIT}, {"spawn", TK_SPAWN},
    
    // Error handling
    {"try", TK_TRY}, {"catch", TK_CATCH}, {"finally", TK_FINALLY},
    {"throw", TK_THROW},
    
    // Types
    {"int", TK_TYPE_INT}, {"float", TK_TYPE_FLOAT}, {"string", TK_TYPE_STR},
    {"bool", TK_TYPE_BOOL}, {"char", TK_TYPE_CHAR}, {"void", TK_TYPE_VOID},
    {"any", TK_TYPE_ANY}, {"auto", TK_TYPE_AUTO}, {"dynamic", TK_TYPE_DYNAMIC},
    {"array", TK_TYPE_ARRAY}, {"map", TK_TYPE_MAP}, {"func", TK_TYPE_FUNC},
    
    // Memory
    {"sizeof", TK_SIZEOF}, {"size", TK_SIZE}, {"siz", TK_SIZ},
    {"free", TK_FREE}, {"gc", TK_GC},
    
    // Debug
    {"db", TK_DB}, {"dbvar", TK_DBVAR}, {"printdb", TK_PRINT_DB},
    {"assert", TK_ASSERT},
    
    // I/O
    {"print", TK_PRINT}, {"input", TK_INPUT}, {"read", TK_READ},
    {"write", TK_WRITE},
    
    // Special
    {"main", TK_MAIN}, {"this", TK_THIS}, {"self", TK_SELF},
    {"super", TK_SUPER}, {"constructor", TK_CONSTRUCTOR},
    {"destructor", TK_DESTRUCTOR}, {"get", TK_GET}, {"set", TK_SET},
    {"init", TK_INIT}, {"deinit", TK_DEINIT},
    
    // Templates
    {"template", TK_TEMPLATE}, {"generic", TK_GENERIC}, {"where", TK_WHERE},
    
    // Literals
    {"true", TK_TRUE}, {"false", TK_FALSE}, {"null", TK_NULL},
    {"undefined", TK_UNDEFINED},
    
    // End marker
    {NULL, TK_ERROR}
};

// ======================================================
// [SECTION] AST NODE DEFINITIONS
// ======================================================
typedef enum {
    // Expressions
    NODE_INT,
    NODE_FLOAT,
    NODE_STRING,
    NODE_CHAR,
    NODE_BOOL,
    NODE_IDENT,
    NODE_NULL,
    NODE_UNDEFINED,
    NODE_ARRAY,
    NODE_MAP,
    NODE_TUPLE,
    
    // Operations
    NODE_BINARY,
    NODE_UNARY,
    NODE_ASSIGN,
    NODE_COMPOUND_ASSIGN,
    
    // Control flow
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_FOREACH,
    NODE_SWITCH,
    NODE_CASE,
    NODE_TRY,
    NODE_THROW,
    
    // Functions
    NODE_FUNC,
    NODE_CALL,
    NODE_RETURN,
    NODE_YIELD,
    
    // Async
    NODE_ASYNC_FUNC,
    NODE_AWAIT,
    NODE_SPAWN,
    
    // Classes
    NODE_CLASS,
    NODE_STRUCT,
    NODE_ENUM,
    NODE_NEW,
    NODE_DELETE,
    NODE_THIS,
    NODE_SUPER,
    NODE_CONSTRUCTOR,
    NODE_MEMBER_ACCESS,
    NODE_METHOD_CALL,
    
    // Variables
    NODE_VAR_DECL,
    NODE_NET_DECL,
    NODE_CLOG_DECL,
    NODE_DOS_DECL,
    NODE_SEL_DECL,
    NODE_CONST_DECL,
    
    // Types
    NODE_TYPE_DECL,
    NODE_TYPEDEF,
    NODE_GENERIC,
    
    // Memory
    NODE_SIZEOF,
    NODE_ALLOC,
    NODE_FREE,
    
    // Modules
    NODE_IMPORT,
    NODE_EXPORT,
    NODE_NAMESPACE,
    NODE_USING,
    
    // Debug
    NODE_DBVAR,
    NODE_ASSERT,
    
    // I/O
    NODE_PRINT,
    NODE_INPUT,
    NODE_READ,
    NODE_WRITE,
    
    // Blocks
    NODE_BLOCK,
    NODE_SCOPE,
    
    // Special
    NODE_MAIN,
    NODE_MODULE,
    NODE_PROGRAM,
    NODE_EMPTY
} NodeType;

// Structure AST Node
typedef struct ASTNode {
    NodeType type;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* third;
    struct ASTNode* fourth;
    
    // Données spécifiques au nœud
    union {
        // Valeurs de base
        int64_t int_val;
        double float_val;
        char* str_val;
        char char_val;
        bool bool_val;
        
        // Identifiants
        char* name;
        
        // Types
        ValueType value_type;
        char* type_name;
        
        // Tableaux et collections
        struct {
            struct ASTNode** elements;
            int element_count;
        } array;
        
        // Maps
        struct {
            char** keys;
            struct ASTNode** values;
            int pair_count;
        } map;
        
        // Fonctions
        struct {
            char** params;
            int param_count;
            struct ASTNode* body;
            bool is_async;
            bool is_generator;
        } func;
        
        // Classes
        struct {
            char* class_name;
            char* parent_class;
            struct ASTNode** members;
            int member_count;
            struct ASTNode** methods;
            int method_count;
        } class_def;
        
        // Import/Export
        struct {
            char** modules;
            char* from_module;
            char* alias;
            int module_count;
            bool is_wildcard;
        } import_export;
        
        // Size info
        struct {
            char* var_name;
            int size_bytes;
        } size_info;
        
        // Loop info
        struct {
            struct ASTNode* init;
            struct ASTNode* condition;
            struct ASTNode* update;
            struct ASTNode* body;
        } loop;
        
        // Async
        struct {
            struct ASTNode* promise;
            struct ASTNode* then_block;
            struct ASTNode* catch_block;
        } async_data;
    } data;
    
    // Informations supplémentaires
    int line;
    int column;
    TokenKind op_type;
    ValueType node_type;
    bool is_constant;
    bool is_mutable;
    int scope_depth;
    char* module_name;
} ASTNode;

// ======================================================
// [SECTION] STRUCTURES POUR LA VM
// ======================================================
typedef struct Value {
    ValueType type;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        char char_val;
        bool bool_val;
        void* ptr_val;
        struct {
            struct Value** elements;
            int length;
            int capacity;
        } array;
        struct {
            char** keys;
            struct Value** values;
            int count;
            int capacity;
        } map;
        struct {
            char* class_name;
            struct Value** fields;
            int field_count;
        } object;
        struct {
            struct ASTNode* func_node;
            struct Value* closure;
        } func;
        struct {
            pthread_t thread;
            struct Value* result;
            bool resolved;
            bool rejected;
            struct Value* error;
        } promise;
    } as;
    bool is_constant;
    char* name;
    int ref_count;
} Value;

typedef struct Variable {
    char name[256];
    Value* value;
    ValueType declared_type;
    bool is_constant;
    bool is_mutable;
    int scope_level;
    char* module;
    bool is_exported;
    struct Variable* next;
} Variable;

typedef struct Scope {
    Variable* variables;
    int level;
    struct Scope* parent;
    struct Scope* child;
} Scope;

typedef struct ClassDefinition {
    char name[256];
    char* parent;
    Variable** fields;
    int field_count;
    struct ASTNode** methods;
    int method_count;
    bool is_abstract;
    bool is_sealed;
    struct ClassDefinition* next;
} ClassDefinition;

typedef struct Package {
    char name[256];
    char* path;
    Variable** exports;
    int export_count;
    ClassDefinition** classes;
    int class_count;
    bool loaded;
    struct Package* next;
} Package;

// ======================================================
// [SECTION] FONCTIONS UTILITAIRES
// ======================================================
static inline char* str_copy(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* dest = malloc(len + 1);
    if (dest) {
        strcpy(dest, src);
    }
    return dest;
}

static inline char* str_ncopy(const char* src, size_t n) {
    if (!src || n == 0) return NULL;
    char* dest = malloc(n + 1);
    if (dest) {
        strncpy(dest, src, n);
        dest[n] = '\0';
    }
    return dest;
}

static inline char* str_format(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    
    // Première passe pour obtenir la taille
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy) + 1;
    va_end(args_copy);
    
    if (size <= 0) {
        va_end(args);
        return NULL;
    }
    
    char* buffer = malloc(size);
    if (buffer) {
        vsnprintf(buffer, size, fmt, args);
    }
    
    va_end(args);
    return buffer;
}

static inline bool str_equal(const char* a, const char* b) {
    if (!a || !b) return a == b;
    return strcmp(a, b) == 0;
}

static inline bool str_startswith(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    size_t len_str = strlen(str);
    size_t len_prefix = strlen(prefix);
    if (len_prefix > len_str) return false;
    return strncmp(str, prefix, len_prefix) == 0;
}

static inline bool str_endswith(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t len_str = strlen(str);
    size_t len_suffix = strlen(suffix);
    if (len_suffix > len_str) return false;
    return strcmp(str + len_str - len_suffix, suffix) == 0;
}

static inline char* str_join(const char** strings, int count, const char* delimiter) {
    if (count == 0) return str_copy("");
    
    // Calculer la longueur totale
    size_t total_len = 0;
    size_t delim_len = delimiter ? strlen(delimiter) : 0;
    
    for (int i = 0; i < count; i++) {
        if (strings[i]) {
            total_len += strlen(strings[i]);
            if (i < count - 1 && delimiter) {
                total_len += delim_len;
            }
        }
    }
    
    // Allouer et construire
    char* result = malloc(total_len + 1);
    if (!result) return NULL;
    
    result[0] = '\0';
    char* current = result;
    
    for (int i = 0; i < count; i++) {
        if (strings[i]) {
            size_t len = strlen(strings[i]);
            memcpy(current, strings[i], len);
            current += len;
            
            if (i < count - 1 && delimiter) {
                memcpy(current, delimiter, delim_len);
                current += delim_len;
            }
        }
    }
    
    *current = '\0';
    return result;
}

// ======================================================
// [SECTION] FONCTIONS DE GESTION D'ERREUR
// ======================================================
#define ERROR_MAX_LENGTH 1024

typedef struct {
    char message[ERROR_MAX_LENGTH];
    int line;
    int column;
    char* module;
} Error;

static inline void set_error(Error* err, int line, int column, const char* module, const char* fmt, ...) {
    if (!err) return;
    
    err->line = line;
    err->column = column;
    
    if (module) {
        err->module = str_copy(module);
    } else {
        err->module = NULL;
    }
    
    va_list args;
    va_start(args, fmt);
    vsnprintf(err->message, ERROR_MAX_LENGTH, fmt, args);
    va_end(args);
}

static inline void print_error(Error* err) {
    if (!err || !err->message[0]) return;
    
    fprintf(stderr, RED "[ERROR]" RESET);
    if (err->module) {
        fprintf(stderr, " in %s", err->module);
    }
    if (err->line > 0) {
        fprintf(stderr, " at line %d, col %d", err->line, err->column);
    }
    fprintf(stderr, ": %s\n", err->message);
}

static inline void clear_error(Error* err) {
    if (!err) return;
    err->message[0] = '\0';
    err->line = 0;
    err->column = 0;
    if (err->module) {
        free(err->module);
        err->module = NULL;
    }
}

// ======================================================
// [SECTION] MACROS UTILES
// ======================================================
#define UNUSED(x) (void)(x)
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define MAX(a, b) ((a) > (b) ? (a) : (b))
#define CLAMP(x, min, max) ((x) < (min) ? (min) : ((x) > (max) ? (max) : (x)))

#define CHECK_ALLOC(ptr) \
    if (!(ptr)) { \
        fprintf(stderr, RED "[FATAL]" RESET " Memory allocation failed at %s:%d\n", __FILE__, __LINE__); \
        exit(EXIT_FAILURE); \
    }

#define RETURN_IF_ERROR(err) \
    if ((err) && (err)->message[0]) { \
        print_error(err); \
        return; \
    }

#define RETURN_VALUE_IF_ERROR(err, retval) \
    if ((err) && (err)->message[0]) { \
        print_error(err); \
        return retval; \
    }

// ======================================================
// [SECTION] DÉCLARATIONS DES FONCTIONS PRINCIPALES
// ======================================================
// Lexer
void init_lexer(const char* source);
Token scan_token();
Token peek_token();
Token previous_token();
bool is_at_end();

// Parser
ASTNode** parse(const char* source, int* node_count, Error* error);
ASTNode* parse_statement();
ASTNode* parse_expression();
ASTNode* parse_declaration();

// VM
void execute(ASTNode* node, Error* error);
Value* evaluate(ASTNode* node, Error* error);
void run(const char* source, const char* filename, Error* error);

// Types
Value* create_value(ValueType type);
void free_value(Value* value);
Value* copy_value(const Value* value);
char* value_to_string(const Value* value);
bool values_equal(const Value* a, const Value* b);

// Classes
ClassDefinition* create_class(const char* name);
void add_class_method(ClassDefinition* cls, ASTNode* method);
void add_class_field(ClassDefinition* cls, const char* name, Value* value);
ClassDefinition* find_class(const char* name);

// Packages
Package* create_package(const char* name, const char* path);
void add_package_export(Package* pkg, Variable* var);
void add_package_class(Package* pkg, ClassDefinition* cls);
Package* find_package(const char* name);
void load_package(const char* name, Error* error);

// Async
Value* create_promise();
void resolve_promise(Value* promise, Value* result);
void reject_promise(Value* promise, Value* error);
Value* await_promise(Value* promise, Error* error);
void* async_thread_func(void* arg);

// Utilitaires
char* read_file(const char* filename, Error* error);
void print_ast(ASTNode* node, int depth);
void print_value(const Value* value);
void print_stack_trace();

#endif // SWIFTFLOW_COMMON_H

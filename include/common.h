#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdarg.h>
#include <limits.h>
#include <errno.h>
#include <time.h>

// ======================================================
// [SECTION] VERSION & CONFIGURATION
// ======================================================
#define SWIFTFLOW_VERSION_MAJOR 1
#define SWIFTFLOW_VERSION_MINOR 0
#define SWIFTFLOW_VERSION_PATCH 0
#define SWIFTFLOW_VERSION_STRING "1.0.0"

#define MAX_IDENT_LENGTH 256
#define MAX_STRING_LENGTH 4096
#define MAX_IMPORT_PATHS 32
#define MAX_MODULE_NAME 128
#define MAX_FUNCTION_PARAMS 32

// ======================================================
// [SECTION] ANSI COLOR CODES
// ======================================================
#define COLOR_RESET   "\033[0m"
#define COLOR_BLACK   "\033[30m"
#define COLOR_RED     "\033[31m"
#define COLOR_GREEN   "\033[32m"
#define COLOR_YELLOW  "\033[33m"
#define COLOR_BLUE    "\033[34m"
#define COLOR_MAGENTA "\033[35m"
#define COLOR_CYAN    "\033[36m"
#define COLOR_WHITE   "\033[37m"
#define COLOR_BRIGHT_BLACK   "\033[90m"
#define COLOR_BRIGHT_RED     "\033[91m"
#define COLOR_BRIGHT_GREEN   "\033[92m"
#define COLOR_BRIGHT_YELLOW  "\033[93m"
#define COLOR_BRIGHT_BLUE    "\033[94m"
#define COLOR_BRIGHT_MAGENTA "\033[95m"
#define COLOR_BRIGHT_CYAN    "\033[96m"
#define COLOR_BRIGHT_WHITE   "\033[97m"

#define BOLD "\033[1m"
#define UNDERLINE "\033[4m"
#define REVERSE "\033[7m"

// ======================================================
// [SECTION] LOGGING SYSTEM
// ======================================================
typedef enum {
    LOG_FATAL,    // Fatal error - program stops
    LOG_ERROR,    // Error - can continue but problematic
    LOG_WARNING,  // Warning - potential issue
    LOG_INFO,     // Informational
    LOG_DEBUG,    // Debug information
    LOG_TRACE     // Detailed trace
} LogLevel;

#define LOG(level, ...) swiftflow_log(level, __FILE__, __LINE__, __VA_ARGS__)

// Log function prototype
void swiftflow_log(LogLevel level, const char* file, int line, const char* fmt, ...);

// ======================================================
// [SECTION] TOKEN DEFINITIONS
// ======================================================
typedef enum {
    // Literals
    TK_INT, TK_FLOAT, TK_STRING, TK_TRUE, TK_FALSE,
    TK_NULL, TK_UNDEFINED, TK_NAN, TK_INF,
    
    // Identifiers
    TK_IDENT, TK_AS, TK_OF,
    
    // Operators
    TK_PLUS, TK_MINUS, TK_MULT, TK_DIV, TK_MOD,
    TK_POW, TK_CONCAT, TK_SPREAD, TK_NULLISH,
    
    // Assignment operators
    TK_ASSIGN, TK_EQ, TK_NEQ, TK_GT, TK_LT, TK_GTE, TK_LTE,
    TK_PLUS_ASSIGN, TK_MINUS_ASSIGN, TK_MULT_ASSIGN, 
    TK_DIV_ASSIGN, TK_MOD_ASSIGN, TK_POW_ASSIGN,
    TK_CONCAT_ASSIGN,
    
    // Logical operators
    TK_AND, TK_OR, TK_NOT,
    
    // Bitwise operators
    TK_BIT_AND, TK_BIT_OR, TK_BIT_XOR, TK_BIT_NOT,
    TK_SHL, TK_SHR, TK_USHR,
    
    // Special operators
    TK_RARROW, TK_DARROW, TK_LDARROW, TK_RDARROW,
    TK_SPACESHIP, TK_ELLIPSIS, TK_RANGE, TK_RANGE_INCL,
    TK_QUESTION, TK_SCOPE, TK_SAFE_NAV,
    
    // Type operators
    TK_IN, TK_IS, TK_ISNOT, TK_AS_OP,
    
    // Punctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_SEMICOLON, TK_COLON, TK_PERIOD,
    TK_AT, TK_HASH, TK_DOLLAR, TK_BACKTICK,
    TK_PIPE, TK_AMPERSAND, TK_CARET, TK_TILDE,
    TK_EXCLAMATION, TK_QUESTION_MARK,
    
    // Brackets for SwiftFlow syntax
    TK_LSQUARE, TK_RSQUARE,  // [] for conditions
    
    // Keywords - Variables
    TK_VAR, TK_LET, TK_CONST,
    TK_NET, TK_CLOG, TK_DOS, TK_SEL,
    
    // Control flow
    TK_THEN, TK_DO,
    TK_IF, TK_ELSE, TK_ELIF,
    TK_WHILE, TK_FOR, TK_SWITCH, TK_CASE, TK_DEFAULT,
    TK_BREAK, TK_CONTINUE, TK_RETURN, TK_YIELD,
    
    // Error handling
    TK_TRY, TK_CATCH, TK_FINALLY, TK_THROW,
    
    // Functions & Modules
    TK_FUNC, TK_IMPORT, TK_EXPORT, TK_FROM,
    TK_CLASS, TK_STRUCT, TK_ENUM, TK_INTERFACE,
    TK_TYPEDEF, TK_TYPELOCK, TK_NAMESPACE,
    
    // Types
    TK_TYPE_INT, TK_TYPE_FLOAT, TK_TYPE_STR,
    TK_TYPE_BOOL, TK_TYPE_CHAR, TK_TYPE_VOID,
    TK_TYPE_ANY, TK_TYPE_AUTO, TK_TYPE_UNKNOWN,
    TK_TYPE_NET, TK_TYPE_CLOG, TK_TYPE_DOS,
    TK_TYPE_SEL, TK_TYPE_ARRAY, TK_TYPE_MAP,
    TK_TYPE_FUNC, TK_DECREMENT, TK_INCREMENT, TK_TYPEOF,
    
    // Memory & Size
    TK_SIZEOF, TK_SIZE, TK_SIZ,
    TK_NEW, TK_DELETE, TK_FREE,
    
    // Debug & DB
    TK_DB, TK_DBVAR, TK_PRINT_DB, TK_ASSERT,
    
    // I/O
    TK_PRINT, TK_WELD, TK_READ, TK_WRITE, TK_INPUT,
    
    // New keywords
    TK_PASS, TK_GLOBAL, TK_LAMBDA,
    TK_BDD, TK_DEF, TK_TYPE, TK_RAISE,
    TK_WITH, TK_LEARN, TK_NONLOCAL, TK_LOCK, 
    TK_APPEND, TK_PUSH, TK_POP, TK_TO,
    
    // JSON & Data
    TK_JSON, TK_YAML, TK_XML,
    
    // Special
    TK_MAIN, TK_THIS, TK_SELF, TK_SUPER,
    TK_STATIC, TK_PUBLIC, TK_PRIVATE, TK_PROTECTED,
    
    // Async
    TK_ASYNC, TK_AWAIT,
    
    // File operations
    TK_FILE_OPEN, TK_FILE_CLOSE, TK_FILE_READ, TK_FILE_WRITE,
    
    // End markers
    TK_EOF, TK_ERROR
} TokenKind;

// Token structure
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
        bool bool_val;
    } value;
} Token;

// Keyword mapping
typedef struct {
    const char* keyword;
    TokenKind kind;
    int length;
} Keyword;

static const Keyword keywords[] = {
    // Variables
    {"var", TK_VAR, 3}, {"let", TK_LET, 3}, {"const", TK_CONST, 5},
    {"net", TK_NET, 3}, {"clog", TK_CLOG, 4}, {"dos", TK_DOS, 3}, {"sel", TK_SEL, 3},
    
    // Control flow
    {"if", TK_IF, 2}, {"else", TK_ELSE, 4}, {"elif", TK_ELIF, 4},
    {"while", TK_WHILE, 5}, {"for", TK_FOR, 3}, {"do", TK_DO, 2},
    {"switch", TK_SWITCH, 6}, {"case", TK_CASE, 4}, {"default", TK_DEFAULT, 7},
    {"break", TK_BREAK, 5}, {"continue", TK_CONTINUE, 8}, {"return", TK_RETURN, 6},
    {"then", TK_THEN, 4}, {"yield", TK_YIELD, 5},
    
    // Error handling
    {"try", TK_TRY, 3}, {"catch", TK_CATCH, 5}, {"finally", TK_FINALLY, 7},
    {"throw", TK_THROW, 5},
    
    // Functions
    {"func", TK_FUNC, 4}, {"import", TK_IMPORT, 6}, {"export", TK_EXPORT, 6},
    {"from", TK_FROM, 4}, {"class", TK_CLASS, 5}, {"struct", TK_STRUCT, 6},
    {"enum", TK_ENUM, 4}, {"interface", TK_INTERFACE, 9}, {"typedef", TK_TYPEDEF, 7},
    {"typelock", TK_TYPELOCK, 8}, {"namespace", TK_NAMESPACE, 9},
    
    // Types
    {"int", TK_TYPE_INT, 3}, {"float", TK_TYPE_FLOAT, 5}, {"string", TK_TYPE_STR, 6},
    {"bool", TK_TYPE_BOOL, 4}, {"char", TK_TYPE_CHAR, 4}, {"void", TK_TYPE_VOID, 4},
    {"any", TK_TYPE_ANY, 3}, {"auto", TK_TYPE_AUTO, 4}, {"unknown", TK_TYPE_UNKNOWN, 7},
    {"netvar", TK_TYPE_NET, 6}, {"clogvar", TK_TYPE_CLOG, 7}, 
    {"dosvar", TK_TYPE_DOS, 6}, {"selvar", TK_TYPE_SEL, 6},
    {"array", TK_TYPE_ARRAY, 5}, {"map", TK_TYPE_MAP, 3}, {"func", TK_TYPE_FUNC, 4},
    
    // Memory
    {"sizeof", TK_SIZEOF, 6}, {"size", TK_SIZE, 4}, {"siz", TK_SIZ, 3},
    {"new", TK_NEW, 3}, {"delete", TK_DELETE, 6}, {"free", TK_FREE, 4},
    
    // Debug
    {"db", TK_DB, 2}, {"dbvar", TK_DBVAR, 5}, {"printdb", TK_PRINT_DB, 7},
    {"assert", TK_ASSERT, 6},
    
    // I/O
    {"print", TK_PRINT, 5}, {"weld", TK_WELD, 4}, {"read", TK_READ, 4},
    {"write", TK_WRITE, 5}, {"input", TK_INPUT, 5},
    
    // New keywords
    {"pass", TK_PASS, 4}, {"global", TK_GLOBAL, 6}, {"lambda", TK_LAMBDA, 6},
    {"bdd", TK_BDD, 3}, {"def", TK_DEF, 3}, {"type", TK_TYPE, 4}, 
    {"raise", TK_RAISE, 5}, {"with", TK_WITH, 4}, {"learn", TK_LEARN, 5},
    {"nonlocal", TK_NONLOCAL, 8}, {"lock", TK_LOCK, 4}, {"append", TK_APPEND, 6},
    {"push", TK_PUSH, 4}, {"pop", TK_POP, 3}, {"to", TK_TO, 2},
    
    // JSON & Data
    {"json", TK_JSON, 4}, {"yaml", TK_YAML, 4}, {"xml", TK_XML, 3},
    
    // Operators as keywords
    {"in", TK_IN, 2}, {"is", TK_IS, 2}, {"isnot", TK_ISNOT, 5}, {"as", TK_AS_OP, 2},
    {"of", TK_OF, 2},
    
    // Special
    {"main", TK_MAIN, 4}, {"this", TK_THIS, 4}, {"self", TK_SELF, 4},
    {"super", TK_SUPER, 5}, {"static", TK_STATIC, 6}, {"public", TK_PUBLIC, 6},
    {"private", TK_PRIVATE, 7}, {"protected", TK_PROTECTED, 9},
    
    // Async
    {"async", TK_ASYNC, 5}, {"await", TK_AWAIT, 5},
    
    // File operations
    {"open", TK_FILE_OPEN, 4}, {"close", TK_FILE_CLOSE, 5},
    {"fread", TK_FILE_READ, 5}, {"fwrite", TK_FILE_WRITE, 6},
    
    // Literals
    {"true", TK_TRUE, 4}, {"false", TK_FALSE, 5}, {"null", TK_NULL, 4},
    {"undefined", TK_UNDEFINED, 9}, {"nan", TK_NAN, 3}, {"inf", TK_INF, 3},
    
    // End marker
    {NULL, TK_ERROR, 0}
};

// ======================================================
// [SECTION] AST NODE DEFINITIONS
// ======================================================
typedef enum {
    // Expressions
    NODE_INT,
    NODE_FLOAT,
    NODE_STRING,
    NODE_BOOL,
    NODE_IDENT,
    NODE_NULL,
    NODE_UNDEFINED,
    NODE_NAN,
    NODE_INF,
    NODE_LIST,
    NODE_TYPE,
    NODE_MAP,
    NODE_FUNC,
    NODE_FUNC_CALL,
    NODE_LAMBDA,
    NODE_ARRAY_ACCESS,
    NODE_MEMBER_ACCESS,
    
    // File operations
    NODE_FILE_OPEN,
    NODE_FILE_CLOSE,
    NODE_FILE_READ,
    NODE_FILE_WRITE,
    NODE_INPUT,
    
    // Operations
    NODE_BINARY,
    NODE_UNARY,
    NODE_TERNARY,
    NODE_ASSIGN,
    NODE_COMPOUND_ASSIGN,
    
    // Control flow
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_FOR_IN,
    NODE_SWITCH,
    NODE_CASE,
    NODE_RETURN,
    NODE_YIELD,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_THROW,
    NODE_TRY,
    NODE_CATCH,
    
    // Variables
    NODE_VAR_DECL,
    NODE_NET_DECL,
    NODE_CLOG_DECL,
    NODE_DOS_DECL,
    NODE_SEL_DECL,
    NODE_CONST_DECL,
    NODE_GLOBAL_DECL,
    NODE_NONLOCAL_DECL,
    
    // Memory
    NODE_SIZEOF,
    NODE_NEW,
    NODE_DELETE,
    NODE_FREE,
    
    // Modules
    NODE_IMPORT,
    NODE_EXPORT,
    NODE_MODULE,
    
    // Debug
    NODE_DBVAR,
    NODE_ASSERT,
    NODE_PRINT_DB,
    
    // I/O
    NODE_PRINT,
    NODE_WELD,
    NODE_READ,
    NODE_WRITE,
    
    // New nodes
    NODE_PASS,
    NODE_WITH,
    NODE_LEARN,
    NODE_LOCK,
    NODE_APPEND,
    NODE_PUSH,
    NODE_POP,
    
    // OOP
    NODE_CLASS,
    NODE_STRUCT,
    NODE_ENUM,
    NODE_INTERFACE,
    NODE_TYPEDEF,
    NODE_NAMESPACE,
    NODE_NEW_INSTANCE,
    NODE_METHOD_CALL,
    NODE_PROPERTY_ACCESS,
    NODE_CLASS_INIT,
    NODE_CLASS_METHOD,
    
    // JSON & Data
    NODE_JSON,
    NODE_YAML,
    NODE_XML,
    
    // Async
    NODE_ASYNC,
    NODE_AWAIT,
    
    // Blocks
    NODE_BLOCK,
    NODE_SCOPE,
    
    // Special
    NODE_MAIN,
    NODE_PROGRAM,
    NODE_EMPTY,
    
    // Type operations
    NODE_TYPEOF,
    NODE_IS,
    NODE_AS,
    
    // Range
    NODE_RANGE,
    NODE_RANGE_INCLUSIVE,
    
    // Spread
    NODE_SPREAD,
    
    // Nullish coalescing
    NODE_NULLISH
} NodeType;

// AST Node structure
typedef struct ASTNode {
    NodeType type;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* third;
    struct ASTNode* fourth;
    
    union {
        // Basic values
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
        
        // Identifiers
        char* name;
        
        // Types
        char* type_name;
        
        // Import/Export
        struct {
            char** modules;
            char* from_module;
            int module_count;
            bool is_selective;  // import "func" from "module"
        } imports;
        
        // Export
        struct {
            char* symbol;
            char* alias;
            bool is_default;
        } export_info;
        
        // Size info
        struct {
            char* var_name;
            int size_bytes;
        } size_info;
        
        // For loop
        struct {
            struct ASTNode* init;
            struct ASTNode* condition;
            struct ASTNode* update;
            struct ASTNode* body;
        } loop;
        
        // For-in loop
        struct {
            char* var_name;
            struct ASTNode* iterable;
            struct ASTNode* body;
        } for_in;
        
        // Append operation
        struct {
            struct ASTNode* list;
            struct ASTNode* value;
        } append_op;
        
        // Push/Pop
        struct {
            struct ASTNode* collection;
            struct ASTNode* value;
        } collection_op;
        
        // Try-Catch
        struct {
            struct ASTNode* try_block;
            struct ASTNode* catch_block;
            struct ASTNode* finally_block;
            char* error_var;
        } try_catch;
        
        // Class/Struct
        struct {
            char* name;
            struct ASTNode* parent;
            struct ASTNode* members;
            struct ASTNode* methods;
            struct ASTNode* static_members;
        } class_def;
        
        // Switch-Case
        struct {
            struct ASTNode* expr;
            struct ASTNode* cases;
            struct ASTNode* default_case;
        } switch_stmt;
        
        // Case
        struct {
            struct ASTNode* value;
            struct ASTNode* body;
            bool is_default;
        } case_stmt;
        
        // JSON/Data
        struct {
            char* data;
            char* format;
        } data_literal;
        
        // Function
        struct {
            char* name;
            struct ASTNode* params;
            struct ASTNode* body;
            struct ASTNode* return_type;
            bool is_async;
            bool is_generator;
        } func_def;
        
        // Function call
        struct {
            struct ASTNode* function;
            struct ASTNode* arguments;
            int arg_count;
        } func_call;
        
        // Binary operation
        struct {
            TokenKind op;
            struct ASTNode* left;
            struct ASTNode* right;
        } binary_op;
        
        // File operation
        struct {
            char* filename;
            char* mode;
            struct ASTNode* content;
        } file_op;
        
        // Input
        struct {
            char* prompt;
        } input_op;
    } data;
    
    // Additional info
    int line;
    int column;
    TokenKind op_type;
} ASTNode;

// ======================================================
// [SECTION] VALUE REPRESENTATION (Runtime)
// ======================================================
typedef enum {
    VAL_INT,
    VAL_FLOAT,
    VAL_BOOL,
    VAL_STRING,
    VAL_NULL,
    VAL_UNDEFINED,
    VAL_NAN,
    VAL_INF,
    VAL_ARRAY,
    VAL_MAP,
    VAL_FUNCTION,
    VAL_CLASS,
    VAL_OBJECT,
    VAL_ERROR
} ValueType;

typedef struct Value Value;
typedef struct Object Object;

struct Value {
    ValueType type;
    union {
        int64_t int_val;
        double float_val;
        bool bool_val;
        char* str_val;
        struct {
            Value* elements;
            int count;
            int capacity;
        } array;
        struct {
            char** keys;
            Value* values;
            int count;
            int capacity;
        } map;
        struct {
            ASTNode* func_node;
            char* name;
            Value* closure;
        } function;
        Object* object;
    } as;
};

struct Object {
    char* class_name;
    Value* fields;
    int field_count;
    struct Object* prototype;
};

// ======================================================
// [SECTION] SYMBOL TABLE
// ======================================================
typedef struct Symbol {
    char* name;
    Value value;
    bool is_constant;
    bool is_global;
    bool is_net;    // For net variables
    bool is_clog;   // For clog variables
    bool is_dos;    // For dos variables
    bool is_sel;    // For sel variables
    int scope_depth;
    int line;
    int column;
    struct Symbol* next;
} Symbol;

typedef struct SymbolTable {
    Symbol* symbols;
    int count;
    int capacity;
    struct SymbolTable* parent;
    int scope_depth;
} SymbolTable;

// ======================================================
// [SECTION] CONFIGURATION STRUCTURE
// ======================================================
typedef struct {
    // Compilation flags
    bool verbose;
    bool debug;
    bool warnings;
    bool optimize;
    bool emit_llvm;
    bool emit_asm;
    bool link;
    bool interpret;  // Run in interpreter mode
    
    // Files
    char* input_file;
    char* output_file;
    
    // Import paths
    char** import_paths;
    int import_path_count;
    
    // Runtime options
    bool gc_enabled;
    int stack_size;
    int heap_size;
    
    // Optimization levels
    int optimization_level;  // 0-3
    
    // Target architecture
    char* target_arch;  // "x86", "x64", "arm", "arm64"
    
    // Output format
    char* output_format;  // "exe", "so", "ll", "asm"
} SwiftFlowConfig;

// ======================================================
// [SECTION] ERROR HANDLING
// ======================================================
typedef struct {
    char* message;
    int line;
    int column;
    char* file;
    bool is_fatal;
} ErrorInfo;

typedef struct {
    ErrorInfo* errors;
    int error_count;
    int error_capacity;
    bool had_error;
} ErrorReporter;

// ======================================================
// [SECTION] MODULE SYSTEM
// ======================================================
typedef struct Module {
    char* name;
    char* path;
    SymbolTable* symbols;
    ASTNode* ast;
    bool is_loaded;
    bool is_stdlib;
    struct Module** dependencies;
    int dep_count;
    struct Module* next;
} Module;

typedef struct ModuleRegistry {
    Module* modules;
    int module_count;
    char* stdlib_path;
} ModuleRegistry;

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static inline char* str_copy(const char* src) {
    if (!src) return NULL;
    size_t len = strlen(src);
    char* dest = malloc(len + 1);
    if (dest) strcpy(dest, src);
    return dest;
}

static inline char* str_ncopy(const char* src, int n) {
    if (!src || n <= 0) return NULL;
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
    
    // Get length
    va_list args_copy;
    va_copy(args_copy, args);
    int len = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);
    
    if (len < 0) {
        va_end(args);
        return NULL;
    }
    
    // Allocate and format
    char* buffer = malloc(len + 1);
    if (buffer) {
        vsnprintf(buffer, len + 1, fmt, args);
    }
    
    va_end(args);
    return buffer;
}

static inline bool str_equal(const char* a, const char* b) {
    if (a == b) return true;
    if (!a || !b) return false;
    return strcmp(a, b) == 0;
}

static inline bool str_startswith(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    return strncmp(str, prefix, strlen(prefix)) == 0;
}

static inline bool str_endswith(const char* str, const char* suffix) {
    if (!str || !suffix) return false;
    size_t str_len = strlen(str);
    size_t suffix_len = strlen(suffix);
    if (suffix_len > str_len) return false;
    return strcmp(str + str_len - suffix_len, suffix) == 0;
}

static inline char* str_trim(char* str) {
    if (!str) return NULL;
    
    // Trim leading spaces
    while (*str && (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r')) {
        str++;
    }
    
    if (*str == 0) return str;
    
    // Trim trailing spaces
    char* end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r')) {
        *end = 0;
        end--;
    }
    
    return str;
}

// Memory management helper
#define ALLOC(type) ((type*)malloc(sizeof(type)))
#define ALLOC_ARRAY(type, count) ((type*)malloc(sizeof(type) * (count)))
#define REALLOC(ptr, type, count) ((type*)realloc(ptr, sizeof(type) * (count)))
#define FREE(ptr) do { if (ptr) { free(ptr); ptr = NULL; } } while(0)

// Error checking macros
#define CHECK_NULL(ptr, msg) \
    do { \
        if (!(ptr)) { \
            LOG(LOG_ERROR, "Null pointer: %s", msg); \
            return NULL; \
        } \
    } while(0)

#define CHECK_ALLOC(ptr, msg) \
    do { \
        if (!(ptr)) { \
            LOG(LOG_FATAL, "Allocation failed: %s", msg); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

#define CHECK_ERROR(cond, msg) \
    do { \
        if (!(cond)) { \
            LOG(LOG_ERROR, "%s", msg); \
            return false; \
        } \
    } while(0)

// ======================================================
// [SECTION] FUNCTION PROTOTYPES (to be implemented)
// ======================================================

// Logging
void swiftflow_log(LogLevel level, const char* file, int line, const char* fmt, ...);

// AST functions (to be defined in ast.c)
ASTNode* ast_new_node(NodeType type, int line, int column);
void ast_free(ASTNode* node);
void ast_print(ASTNode* node, int indent);
const char* node_type_to_string(NodeType type);
const char* token_kind_to_string(TokenKind kind);

// Value functions
Value value_make_int(int64_t val);
Value value_make_float(double val);
Value value_make_bool(bool val);
Value value_make_string(const char* val);
Value value_make_null(void);
Value value_make_undefined(void);
void value_free(Value* value);
char* value_to_string(Value value);

// Symbol table functions
SymbolTable* symbol_table_new(SymbolTable* parent);
void symbol_table_free(SymbolTable* table);
Symbol* symbol_table_add(SymbolTable* table, const char* name, Value value);
Symbol* symbol_table_get(SymbolTable* table, const char* name);
bool symbol_table_exists(SymbolTable* table, const char* name);
void symbol_table_enter_scope(SymbolTable** table);
void symbol_table_exit_scope(SymbolTable** table);

// Module functions
ModuleRegistry* module_registry_new(void);
void module_registry_free(ModuleRegistry* registry);
Module* module_load(ModuleRegistry* registry, const char* name, const char* from_file);
Module* module_find(ModuleRegistry* registry, const char* name);
bool module_register(ModuleRegistry* registry, Module* module);

// Configuration
SwiftFlowConfig* config_create_default(void);
void config_free(SwiftFlowConfig* config);
bool config_add_import_path(SwiftFlowConfig* config, const char* path);
char* config_resolve_import(SwiftFlowConfi* config, const char* module_name, const char* from_file);

#endif // COMMON_H

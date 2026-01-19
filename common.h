#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <time.h>
#include <math.h>
#include <errno.h>

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
#define RESET   "\033[0m"

// ======================================================
// [SECTION] TOKEN DEFINITIONS (EXTENDED)
// ======================================================
typedef enum {
    // Literals
    TK_INT, TK_FLOAT, TK_STRING, TK_TRUE, TK_FALSE,
    TK_NULL, TK_UNDEFINED, TK_NAN, TK_INF,
    
    // Identifiers
    TK_IDENT, TK_AS, TK_IN, TK_IS, TK_OF,
    
    // Operators
    TK_PLUS, TK_MINUS, TK_MULT, TK_DIV, TK_MOD,
    TK_POW, TK_CONCAT, TK_INC, TK_DEC,
    TK_ASSIGN, TK_EQ, TK_NEQ, TK_GT, TK_LT, TK_GTE, TK_LTE,
    TK_AND, TK_OR, TK_NOT, TK_XOR,
    TK_BIT_AND, TK_BIT_OR, TK_BIT_XOR, TK_BIT_NOT,
    TK_SHL, TK_SHR, TK_BIT_SHL, TK_BIT_SHR,
    
    // Assignment operators
    TK_PLUS_ASSIGN, TK_MINUS_ASSIGN, TK_MULT_ASSIGN, 
    TK_DIV_ASSIGN, TK_MOD_ASSIGN, TK_POW_ASSIGN,
    TK_AND_ASSIGN, TK_OR_ASSIGN, TK_XOR_ASSIGN,
    TK_SHL_ASSIGN, TK_SHR_ASSIGN, TK_TERNARY,
    
    // Special operators
    TK_RARROW, TK_DARROW, TK_LDARROW, TK_RDARROW,
    TK_SPACESHIP, TK_ELLIPSIS, TK_RANGE, TK_PIPE,
    TK_QUESTION, TK_SCOPE, TK_SAFE_NAV,
    
    // Punctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET, TK_LANGLE, TK_RANGLE,
    TK_COMMA, TK_SEMICOLON, TK_COLON, TK_PERIOD,
    TK_AT, TK_HASH, TK_DOLLAR, TK_BACKTICK,
    TK_QUOTE, TK_DQUOTE, TK_BACKSLASH,
    
    // Keywords - Variables
    TK_VAR, TK_LET, TK_CONST, TK_STATIC,
    TK_NET, TK_CLOG, TK_DOS, TK_SEL, TK_REF,
    TK_GLOBAL, TK_LOCAL, TK_THREADLOCAL,
    
    // Keywords - Control flow
    TK_IF, TK_ELSE, TK_ELIF, TK_THEN,
    TK_WHILE, TK_FOR, TK_DO, TK_LOOP,
    TK_SWITCH, TK_CASE, TK_DEFAULT, TK_MATCH,
    TK_BREAK, TK_CONTINUE, TK_RETURN, TK_YIELD,
    TK_GOTO, TK_LABEL,
    
    // Keywords - Exception
    TK_TRY, TK_CATCH, TK_FINALLY, TK_THROW,
    TK_RAISE, TK_ASSERT, TK_REQUIRE, TK_ENSURE,
    
    // Keywords - Functions & Modules
    TK_FUNC, TK_PROC, TK_METHOD, TK_CONSTRUCTOR,
    TK_IMPORT, TK_EXPORT, TK_FROM, TK_USE,
    TK_CLASS, TK_STRUCT, TK_ENUM, TK_UNION,
    TK_INTERFACE, TK_TRAIT, TK_PROTOCOL,
    TK_TYPEDEF, TK_TYPELOCK, TK_NAMESPACE,
    TK_MODULE, TK_PACKAGE, TK_LIBRARY,
    
    // Keywords - Types
    TK_TYPE_INT, TK_TYPE_FLOAT, TK_TYPE_STR,
    TK_TYPE_BOOL, TK_TYPE_CHAR, TK_TYPE_VOID,
    TK_TYPE_ANY, TK_TYPE_AUTO, TK_TYPE_DYNAMIC,
    TK_TYPE_NET, TK_TYPE_CLOG, TK_TYPE_DOS,
    TK_TYPE_SEL, TK_TYPE_REF, TK_TYPE_PTR,
    TK_TYPE_ARRAY, TK_TYPE_LIST, TK_TYPE_MAP,
    TK_TYPE_SET, TK_TYPE_TUPLE, TK_TYPE_OPTION,
    TK_TYPE_RESULT,
    
    // Keywords - Memory
    TK_SIZEOF, TK_SIZE, TK_SIZ, TK_ALIGNOF,
    TK_NEW, TK_DELETE, TK_FREE, TK_ALLOC,
    TK_MALLOC, TK_CALLOC, TK_REALLOC,
    TK_MOVE, TK_COPY, TK_CLONE, TK_DROP,
    
    // Keywords - Debug & Testing
    TK_DB, TK_DBVAR, TK_PRINT_DB, TK_DEBUG,
    TK_TEST, TK_BENCHMARK, TK_ASSERT_EQ,
    TK_ASSERT_NE, TK_ASSERT_GT, TK_ASSERT_LT,
    
    // Keywords - I/O
    TK_PRINT, TK_INPUT, TK_READ, TK_WRITE,
    TK_OPEN, TK_CLOSE, TK_SEEK, TK_TELL,
    TK_FLUSH, TK_EOF,
    
    // Keywords - Concurrency
    TK_ASYNC, TK_AWAIT, TK_SPAWN, TK_TASK,
    TK_CHANNEL, TK_MUTEX, TK_SEMAPHORE,
    TK_LOCK, TK_UNLOCK, TK_BARRIER,
    
    // Keywords - Time
    TK_SLEEP, TK_DELAY, TK_TIMEOUT,
    TK_NOW, TK_TODAY, TK_YESTERDAY,
    
    // Special keywords
    TK_MAIN, TK_THIS, TK_SELF, TK_SUPER,
    TK_BASE, TK_PARENT, TK_ROOT,
    
    // Preprocessor-like
    TK_DEFINE, TK_UNDEF, TK_IFDEF, TK_IFNDEF,
    TK_ELIFDEF, TK_ENDIF, TK_INCLUDE,
    
    // Documentation
    TK_DOC, TK_COMMENT_DOC, TK_DOC_PARAM,
    TK_DOC_RETURN, TK_DOC_THROWS,
    
    // End markers
    TK_ERROR, TK_WARNING, TK_INFO
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
        void* ptr_val;
    } value;
} Token;

// Keyword mapping
typedef struct {
    const char* keyword;
    TokenKind kind;
} Keyword;

static const Keyword keywords[] = {
    // Variables
    {"var", TK_VAR}, {"let", TK_LET}, {"const", TK_CONST}, {"static", TK_STATIC},
    {"net", TK_NET}, {"clog", TK_CLOG}, {"dos", TK_DOS}, {"sel", TK_SEL},
    {"ref", TK_REF}, {"global", TK_GLOBAL}, {"local", TK_LOCAL},
    {"threadlocal", TK_THREADLOCAL},
    
    // Control flow
    {"if", TK_IF}, {"else", TK_ELSE}, {"elif", TK_ELIF}, {"then", TK_THEN},
    {"while", TK_WHILE}, {"for", TK_FOR}, {"do", TK_DO}, {"loop", TK_LOOP},
    {"switch", TK_SWITCH}, {"case", TK_CASE}, {"default", TK_DEFAULT},
    {"match", TK_MATCH}, {"break", TK_BREAK}, {"continue", TK_CONTINUE},
    {"return", TK_RETURN}, {"yield", TK_YIELD}, {"goto", TK_GOTO},
    {"label", TK_LABEL},
    
    // Exception handling
    {"try", TK_TRY}, {"catch", TK_CATCH}, {"finally", TK_FINALLY},
    {"throw", TK_THROW}, {"raise", TK_RAISE}, {"assert", TK_ASSERT},
    {"require", TK_REQUIRE}, {"ensure", TK_ENSURE},
    
    // Functions & Modules
    {"func", TK_FUNC}, {"proc", TK_PROC}, {"method", TK_METHOD},
    {"constructor", TK_CONSTRUCTOR}, {"import", TK_IMPORT},
    {"export", TK_EXPORT}, {"from", TK_FROM}, {"use", TK_USE},
    {"class", TK_CLASS}, {"struct", TK_STRUCT}, {"enum", TK_ENUM},
    {"union", TK_UNION}, {"interface", TK_INTERFACE},
    {"trait", TK_TRAIT}, {"protocol", TK_PROTOCOL},
    {"typedef", TK_TYPEDEF}, {"typelock", TK_TYPELOCK},
    {"namespace", TK_NAMESPACE}, {"module", TK_MODULE},
    {"package", TK_PACKAGE}, {"library", TK_LIBRARY},
    
    // Types
    {"int", TK_TYPE_INT}, {"float", TK_TYPE_FLOAT}, {"string", TK_TYPE_STR},
    {"bool", TK_TYPE_BOOL}, {"char", TK_TYPE_CHAR}, {"void", TK_TYPE_VOID},
    {"any", TK_TYPE_ANY}, {"auto", TK_TYPE_AUTO}, {"dynamic", TK_TYPE_DYNAMIC},
    {"netvar", TK_TYPE_NET}, {"clogvar", TK_TYPE_CLOG},
    {"dosvar", TK_TYPE_DOS}, {"selvar", TK_TYPE_SEL},
    {"refvar", TK_TYPE_REF}, {"ptr", TK_TYPE_PTR},
    {"array", TK_TYPE_ARRAY}, {"list", TK_TYPE_LIST},
    {"map", TK_TYPE_MAP}, {"set", TK_TYPE_SET},
    {"tuple", TK_TYPE_TUPLE}, {"option", TK_TYPE_OPTION},
    {"result", TK_TYPE_RESULT},
    
    // Memory management
    {"sizeof", TK_SIZEOF}, {"size", TK_SIZE}, {"siz", TK_SIZ},
    {"alignof", TK_ALIGNOF}, {"new", TK_NEW}, {"delete", TK_DELETE},
    {"free", TK_FREE}, {"alloc", TK_ALLOC}, {"malloc", TK_MALLOC},
    {"calloc", TK_CALLOC}, {"realloc", TK_REALLOC},
    {"move", TK_MOVE}, {"copy", TK_COPY}, {"clone", TK_CLONE},
    {"drop", TK_DROP},
    
    // Debug & Testing
    {"db", TK_DB}, {"dbvar", TK_DBVAR}, {"printdb", TK_PRINT_DB},
    {"debug", TK_DEBUG}, {"test", TK_TEST}, {"benchmark", TK_BENCHMARK},
    {"assert_eq", TK_ASSERT_EQ}, {"assert_ne", TK_ASSERT_NE},
    {"assert_gt", TK_ASSERT_GT}, {"assert_lt", TK_ASSERT_LT},
    
    // I/O operations
    {"print", TK_PRINT}, {"input", TK_INPUT}, {"read", TK_READ},
    {"write", TK_WRITE}, {"open", TK_OPEN}, {"close", TK_CLOSE},
    {"seek", TK_SEEK}, {"tell", TK_TELL}, {"flush", TK_FLUSH},
    {"eof", TK_EOF},
    
    // Concurrency
    {"async", TK_ASYNC}, {"await", TK_AWAIT}, {"spawn", TK_SPAWN},
    {"task", TK_TASK}, {"channel", TK_CHANNEL}, {"mutex", TK_MUTEX},
    {"semaphore", TK_SEMAPHORE}, {"lock", TK_LOCK}, {"unlock", TK_UNLOCK},
    {"barrier", TK_BARRIER},
    
    // Time operations
    {"sleep", TK_SLEEP}, {"delay", TK_DELAY}, {"timeout", TK_TIMEOUT},
    {"now", TK_NOW}, {"today", TK_TODAY}, {"yesterday", TK_YESTERDAY},
    
    // Special identifiers
    {"main", TK_MAIN}, {"this", TK_THIS}, {"self", TK_SELF},
    {"super", TK_SUPER}, {"base", TK_BASE}, {"parent", TK_PARENT},
    {"root", TK_ROOT},
    
    // Preprocessor
    {"define", TK_DEFINE}, {"undef", TK_UNDEF}, {"ifdef", TK_IFDEF},
    {"ifndef", TK_IFNDEF}, {"elifdef", TK_ELIFDEF},
    {"endif", TK_ENDIF}, {"include", TK_INCLUDE},
    
    // Documentation
    {"doc", TK_DOC}, {"docparam", TK_DOC_PARAM},
    {"docreturn", TK_DOC_RETURN}, {"docthrows", TK_DOC_THROWS},
    
    // Literals
    {"true", TK_TRUE}, {"false", TK_FALSE}, {"null", TK_NULL},
    {"undefined", TK_UNDEFINED}, {"nan", TK_NAN}, {"inf", TK_INF},
    {"Infinity", TK_INF},
    
    // End marker
    {NULL, TK_ERROR}
};

// ======================================================
// [SECTION] AST NODE DEFINITIONS (EXTENDED)
// ======================================================
typedef enum {
    // Expressions
    NODE_INT, NODE_FLOAT, NODE_STRING, NODE_BOOL,
    NODE_IDENT, NODE_NULL, NODE_UNDEFINED, NODE_NAN,
    NODE_ARRAY, NODE_MAP, NODE_TUPLE, NODE_RANGE,
    
    // Operations
    NODE_BINARY, NODE_UNARY, NODE_TERNARY,
    NODE_ASSIGN, NODE_COMPOUND_ASSIGN,
    
    // Control flow
    NODE_IF, NODE_WHILE, NODE_FOR, NODE_LOOP,
    NODE_SWITCH, NODE_CASE, NODE_MATCH,
    NODE_RETURN, NODE_YIELD, NODE_BREAK, NODE_CONTINUE,
    
    // Exception handling
    NODE_TRY, NODE_CATCH, NODE_THROW, NODE_ASSERT,
    
    // Variables
    NODE_VAR_DECL, NODE_NET_DECL, NODE_CLOG_DECL,
    NODE_DOS_DECL, NODE_SEL_DECL, NODE_CONST_DECL,
    NODE_STATIC_DECL, NODE_REF_DECL,
    
    // Functions
    NODE_FUNC_DECL, NODE_FUNC_CALL, NODE_LAMBDA,
    NODE_PARAM_LIST, NODE_ARG_LIST,
    
    // Memory
    NODE_SIZEOF, NODE_ALLOC, NODE_FREE,
    NODE_MOVE, NODE_COPY, NODE_CLONE,
    
    // Modules
    NODE_IMPORT, NODE_EXPORT, NODE_MODULE,
    NODE_PACKAGE, NODE_NAMESPACE,
    
    // Classes & Structs
    NODE_CLASS_DECL, NODE_STRUCT_DECL,
    NODE_ENUM_DECL, NODE_INTERFACE_DECL,
    NODE_FIELD_DECL, NODE_METHOD_DECL,
    
    // Debug
    NODE_DBVAR, NODE_PRINT_DB, NODE_DEBUG,
    NODE_TEST, NODE_BENCHMARK,
    
    // I/O
    NODE_PRINT, NODE_INPUT, NODE_READ, NODE_WRITE,
    
    // Concurrency
    NODE_ASYNC, NODE_AWAIT, NODE_SPAWN,
    NODE_CHANNEL, NODE_MUTEX,
    
    // Time
    NODE_SLEEP, NODE_DELAY, NODE_NOW,
    
    // Special
    NODE_MAIN, NODE_THIS, NODE_SELF,
    
    // Blocks & Scopes
    NODE_BLOCK, NODE_SCOPE,
    
    // Preprocessor
    NODE_DEFINE, NODE_IFDEF, NODE_INCLUDE,
    
    // Empty
    NODE_EMPTY, NODE_COMMENT, NODE_DOC_COMMENT
} NodeType;

// AST Node structure
typedef struct ASTNode {
    NodeType type;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* third;
    struct ASTNode* fourth;
    struct ASTNode* fifth;
    
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
            char* alias;
            int module_count;
            bool is_wildcard;
        } imports;
        
        // Export
        struct {
            char* symbol;
            char* alias;
            bool is_default;
        } export;
        
        // Size info
        struct {
            char* var_name;
            int size_bytes;
            int alignment;
        } size_info;
        
        // For loop
        struct {
            struct ASTNode* init;
            struct ASTNode* condition;
            struct ASTNode* update;
            struct ASTNode* body;
        } loop;
        
        // Function
        struct {
            char* func_name;
            struct ASTNode* params;
            struct ASTNode* body;
            struct ASTNode* return_type;
            bool is_async;
            bool is_extern;
        } func;
        
        // Class/Struct
        struct {
            char* class_name;
            struct ASTNode* parent;
            struct ASTNode* fields;
            struct ASTNode* methods;
            bool is_class; // true for class, false for struct
        } class_def;
        
        // Array/Map
        struct {
            struct ASTNode** elements;
            int element_count;
            char* key_type;
            char* value_type;
        } collection;
        
        // Test/Benchmark
        struct {
            char* test_name;
            struct ASTNode* setup;
            struct ASTNode* teardown;
            struct ASTNode* test_body;
            bool is_benchmark;
        } test;
    } data;
    
    // Additional info
    int line;
    int column;
    TokenKind op_type;
    char* doc_comment;
    bool is_public;
    bool is_private;
    bool is_protected;
} ASTNode;

// ======================================================
// [SECTION] HELPER FUNCTIONS
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

static inline char* str_ncopy(const char* src, int n) {
    if (!src || n <= 0) return NULL;
    char* dest = malloc(n + 1);
    if (dest) {
        strncpy(dest, src, n);
        dest[n] = '\0';
    }
    return dest;
}

static inline char* str_concat(const char* s1, const char* s2) {
    if (!s1 && !s2) return NULL;
    if (!s1) return str_copy(s2);
    if (!s2) return str_copy(s1);
    
    size_t len1 = strlen(s1);
    size_t len2 = strlen(s2);
    char* result = malloc(len1 + len2 + 1);
    if (result) {
        strcpy(result, s1);
        strcat(result, s2);
    }
    return result;
}

static inline char* str_format(const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    // Determine length
    va_list args_copy;
    va_copy(args_copy, args);
    int length = vsnprintf(NULL, 0, format, args_copy);
    va_end(args_copy);
    
    if (length < 0) {
        va_end(args);
        return NULL;
    }
    
    char* buffer = malloc(length + 1);
    if (buffer) {
        vsnprintf(buffer, length + 1, format, args);
    }
    
    va_end(args);
    return buffer;
}

// ======================================================
// [SECTION] ERROR HANDLING
// ======================================================
typedef struct {
    char* message;
    int line;
    int column;
    char* file;
    bool is_warning;
    bool is_fatal;
} ErrorInfo;

static inline void log_error(const char* file, int line, int col, 
                             const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    fprintf(stderr, RED "[ERROR]" RESET " %s:%d:%d: ", file, line, col);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}

static inline void log_warning(const char* file, int line, int col,
                               const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    fprintf(stderr, YELLOW "[WARNING]" RESET " %s:%d:%d: ", file, line, col);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}

static inline void log_info(const char* file, int line, int col,
                            const char* format, ...) {
    va_list args;
    va_start(args, format);
    
    fprintf(stderr, CYAN "[INFO]" RESET " %s:%d:%d: ", file, line, col);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");
    
    va_end(args);
}

#endif // COMMON_H

#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>

// ======================================================
// [SECTION] COULEURS POUR LE TERMINAL
// ======================================================
#define RED     "\033[1;31m"
#define GREEN   "\033[1;32m"
#define YELLOW  "\033[1;33m"
#define BLUE    "\033[1;34m"
#define MAGENTA "\033[1;35m"
#define CYAN    "\033[1;36m"
#define RESET   "\033[0m"

// ======================================================
// [SECTION] TOKEN DEFINITIONS
// ======================================================
typedef enum {
    // Literals
    TK_INT, TK_FLOAT, TK_STRING, TK_TRUE, TK_FALSE,
    TK_NULL, TK_UNDEFINED,
    
    // Identifiers
    TK_IDENT,
    
    // Operators
    TK_PLUS, TK_MINUS, TK_MULT, TK_DIV, TK_MOD,
    TK_POW, TK_ROOT, TK_CONCAT,
    
    // Assignment
    TK_ASSIGN, TK_PLUS_ASSIGN, TK_MINUS_ASSIGN,
    TK_MULT_ASSIGN, TK_DIV_ASSIGN, TK_MOD_ASSIGN,
    
    // Comparison
    TK_EQ, TK_NEQ, TK_GT, TK_LT, TK_GTE, TK_LTE,
    TK_AND, TK_OR, TK_NOT,
    TK_SPACESHIP, // <=>
    
    // Bitwise
    TK_BIT_AND, TK_BIT_OR, TK_BIT_XOR, TK_BIT_NOT,
    TK_SHL, TK_SHR, // << >>
    
    // Arrow operators
    TK_LARROW,    // <
    TK_RARROW,    // >
    TK_DARROW,    // ==>
    TK_LDARROW,   // <==
    TK_RDARROW,   // ==>
    
    // Punctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET,
    TK_COMMA, TK_SEMICOLON, TK_COLON, TK_PERIOD,
    TK_AT, TK_HASH, TK_DOLLAR, TK_QUESTION,
    TK_PIPE, TK_AMPERSAND, TK_TILDE, TK_CARET,
    
    // Keywords
    TK_VAR, TK_LET, TK_CONST,
    TK_NET, TK_CLOG, TK_DOS, TK_SEL,
    
    // Control flow
    TK_IF, TK_ELSE, TK_ELIF,
    TK_WHILE, TK_FOR, TK_DO,
    TK_SWITCH, TK_CASE, TK_DEFAULT,
    TK_BREAK, TK_CONTINUE, TK_RETURN,
    
    // Functions & Modules
    TK_FUNC, TK_IMPORT, TK_EXPORT, TK_FROM,
    TK_CLASS, TK_STRUCT, TK_ENUM, TK_TYPEDEF,
    TK_TYPELOCK, TK_NAMESPACE,
    TK_PUBLIC, TK_PRIVATE, TK_PROTECTED,
    
    // Types
    TK_TYPE_INT, TK_TYPE_FLOAT, TK_TYPE_STR,
    TK_TYPE_BOOL, TK_TYPE_CHAR, TK_TYPE_VOID,
    TK_TYPE_ANY, TK_TYPE_AUTO,
    
    // Special types
    TK_TYPE_NET, TK_TYPE_CLOG, TK_TYPE_DOS,
    TK_TYPE_SEL, TK_TYPE_MODULE,
    
    // Memory & Size
    TK_SIZEOF, TK_SIZE, TK_SIZ,
    TK_NEW, TK_DELETE, TK_FREE,
    TK_ALLOC, TK_REALLOC,
    
    // Debug & DB
    TK_DB, TK_DBVAR, TK_PRINT_DB, TK_TRACE,
    
    // I/O
    TK_PRINT, TK_INPUT, TK_READ, TK_WRITE,
    
    // Special
    TK_MAIN, TK_THIS, TK_SUPER, TK_SELF,
    TK_INIT, TK_DEINIT, TK_CONSTRUCTOR, TK_DESTRUCTOR,
    // Dans l'énumération TokenKind (common.h), ajoutez :
    TK_ELLIPSIS,    // ...
    TK_RANGE,       // ..
    TK_SCOPE,       // ::
    // End markers
    TK_EOF, TK_ERROR, TK_UNKNOWN
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
} Keyword;

static const Keyword keywords[] = {
    // Variables
    {"var", TK_VAR}, {"let", TK_LET}, {"const", TK_CONST},
    {"net", TK_NET}, {"clog", TK_CLOG}, {"dos", TK_DOS},
    {"sel", TK_SEL},
    
    // Control flow
    {"if", TK_IF}, {"else", TK_ELSE}, {"elif", TK_ELIF},
    {"while", TK_WHILE}, {"for", TK_FOR}, {"do", TK_DO},
    {"switch", TK_SWITCH}, {"case", TK_CASE}, {"default", TK_DEFAULT},
    {"break", TK_BREAK}, {"continue", TK_CONTINUE}, {"return", TK_RETURN},
    
    // Functions
    {"func", TK_FUNC}, {"import", TK_IMPORT}, {"export", TK_EXPORT},
    {"from", TK_FROM}, {"class", TK_CLASS}, {"struct", TK_STRUCT},
    {"enum", TK_ENUM}, {"typedef", TK_TYPEDEF}, {"typelock", TK_TYPELOCK},
    {"namespace", TK_NAMESPACE},
    
    // Access modifiers
    {"public", TK_PUBLIC}, {"private", TK_PRIVATE}, {"protected", TK_PROTECTED},
    
    // Types
    {"int", TK_TYPE_INT}, {"float", TK_TYPE_FLOAT}, {"string", TK_TYPE_STR},
    {"bool", TK_TYPE_BOOL}, {"char", TK_TYPE_CHAR}, {"void", TK_TYPE_VOID},
    {"any", TK_TYPE_ANY}, {"auto", TK_TYPE_AUTO},
    {"netvar", TK_TYPE_NET}, {"clogvar", TK_TYPE_CLOG}, {"dosvar", TK_TYPE_DOS},
    {"selvar", TK_TYPE_SEL}, {"module", TK_TYPE_MODULE},
    
    // Memory
    {"sizeof", TK_SIZEOF}, {"size", TK_SIZE}, {"siz", TK_SIZ},
    {"new", TK_NEW}, {"delete", TK_DELETE}, {"free", TK_FREE},
    {"alloc", TK_ALLOC}, {"realloc", TK_REALLOC},
    
    // Debug
    {"db", TK_DB}, {"dbvar", TK_DBVAR}, {"printdb", TK_PRINT_DB},
    {"trace", TK_TRACE},
    
    // I/O
    {"print", TK_PRINT}, {"input", TK_INPUT}, {"read", TK_READ},
    {"write", TK_WRITE},
    
    // Special
    {"main", TK_MAIN}, {"this", TK_THIS}, {"super", TK_SUPER},
    {"self", TK_SELF}, {"init", TK_INIT}, {"deinit", TK_DEINIT},
    {"constructor", TK_CONSTRUCTOR}, {"destructor", TK_DESTRUCTOR},
    
    // Literals
    {"true", TK_TRUE}, {"false", TK_FALSE},
    {"null", TK_NULL}, {"undefined", TK_UNDEFINED},
    
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
    NODE_BOOL,
    NODE_IDENT,
    NODE_NULL,
    NODE_UNDEFINED,
    
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
    NODE_DO_WHILE,
    NODE_SWITCH,
    NODE_CASE,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_RETURN,
    
    // Functions
    NODE_FUNC_DECL,
    NODE_FUNC_CALL,
    NODE_LAMBDA,
    
    // Classes & Structs
    NODE_CLASS,
    NODE_STRUCT,
    NODE_CLASS_INSTANCE,
    NODE_MEMBER_ACCESS,
    NODE_METHOD_CALL,
    NODE_CONSTRUCTOR,
    NODE_DESTRUCTOR,
    
    // Variables
    NODE_VAR_DECL,
    NODE_NET_DECL,
    NODE_CLOG_DECL,
    NODE_DOS_DECL,
    NODE_SEL_DECL,
    NODE_CONST_DECL,
    
    // Memory
    NODE_SIZEOF,
    NODE_ALLOC,
    NODE_FREE,
    
    // Modules
    NODE_IMPORT,
    NODE_EXPORT,
    NODE_MODULE,
    
    // Debug
    NODE_DBVAR,
    NODE_PRINT_DB,
    NODE_TRACE,
    
    // I/O
    NODE_PRINT,
    NODE_INPUT,
    
    // Blocks
    NODE_BLOCK,
    NODE_PROGRAM,
    
    // Special
    NODE_MAIN,
    NODE_TYPELOCK,
    NODE_NAMESPACE,
    NODE_EMPTY
} NodeType;

// Function parameter
typedef struct FuncParam {
    char* name;
    char* type;
    struct FuncParam* next;
} FuncParam;

// Class member
typedef struct ClassMember {
    char* name;
    char* type;
    char* visibility; // public, private, protected
    bool is_static;
    bool is_const;
    struct ClassMember* next;
} ClassMember;

// AST Node structure
typedef struct ASTNode {
    NodeType type;
    struct ASTNode* left;
    struct ASTNode* right;
    struct ASTNode* third; // For ternary or other 3-operand nodes
    struct ASTNode* extra; // For additional data
    
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
        
        // Function/Class data
        struct {
            char* name;
            FuncParam* params;
            char* return_type;
            struct ASTNode* body;
            int param_count;
        } func;
        
        struct {
            char* name;
            ClassMember* members;
            struct ASTNode* constructor;
            struct ASTNode* destructor;
            int member_count;
            char* parent_class;
        } class_def;
        
        // Import/Export
        struct {
            char** modules;
            char* from_module;
            int module_count;
        } imports;
        
        // Size info
        struct {
            char* var_name;
            int size_bytes;
        } size_info;
    } data;
    
    // Additional info
    int line;
    int column;
    TokenKind op_type; // For operators
    bool is_exported;
    bool is_global;
} ASTNode;

// ======================================================
// [SECTION] SYMBOL TABLE
// ======================================================
typedef enum {
    SYM_VARIABLE,
    SYM_FUNCTION,
    SYM_CLASS,
    SYM_MODULE,
    SYM_NAMESPACE,
    SYM_PARAMETER,
    SYM_MEMBER
} SymbolType;

typedef struct Symbol {
    char* name;
    SymbolType type;
    char* data_type;
    int size_bytes;
    int scope_level;
    int line_declared;
    bool is_initialized;
    bool is_constant;
    bool is_exported;
    union {
        int64_t int_val;
        double float_val;
        char* str_val;
        bool bool_val;
        ASTNode* func_node;
        ASTNode* class_node;
    } value;
    struct Symbol* next;
} Symbol;

typedef struct Scope {
    int level;
    Symbol* symbols;
    struct Scope* parent;
} Scope;

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
    
    // Determine size needed
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

// ======================================================
// [SECTION] DBVAR TABLE STRUCTURE
// ======================================================
typedef struct {
    char* name;
    char* type;
    int size_bytes;
    char* value_str;
    bool is_initialized;
    int line;
    int scope;
} DBVarEntry;

typedef struct {
    DBVarEntry* entries;
    int count;
    int capacity;
} DBVarTable;

#endif // COMMON_H

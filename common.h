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
    
    // Keywords
    TK_VAR, TK_LET, TK_CONST,
    TK_NET, TK_CLOG, TK_DOS, TK_SEL,
    TK_THEN, TK_DO,
    
    // Control flow
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
    TK_PRINT, TK_WELD, TK_READ, TK_WRITE,
    
    // New keywords
    TK_PASS, TK_GLOBAL, TK_LAMBDA,
    TK_BDD, TK_DEF, TK_TYPE, TK_RAISE,
    TK_WITH, TK_LEARN, TK_NONLOCAL, TK_LOCK, 
    TK_APPEND, TK_PUSH, TK_POP,
    
    // JSON & Data
    TK_JSON, TK_YAML, TK_XML,
    
    // Special
    TK_MAIN, TK_THIS, TK_SELF, TK_SUPER,
    TK_STATIC, TK_PUBLIC, TK_PRIVATE, TK_PROTECTED,
    
    // Async
    TK_ASYNC, TK_AWAIT,
    
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
} Keyword;

static const Keyword keywords[] = {
    // Variables
    {"var", TK_VAR}, {"let", TK_LET}, {"const", TK_CONST},
    {"net", TK_NET}, {"clog", TK_CLOG}, {"dos", TK_DOS}, {"sel", TK_SEL},
    
    // Control flow
    {"if", TK_IF}, {"else", TK_ELSE}, {"elif", TK_ELIF},
    {"while", TK_WHILE}, {"for", TK_FOR}, {"do", TK_DO},
    {"switch", TK_SWITCH}, {"case", TK_CASE}, {"default", TK_DEFAULT},
    {"break", TK_BREAK}, {"continue", TK_CONTINUE}, {"return", TK_RETURN},
    {"then", TK_THEN}, {"yield", TK_YIELD},
    
    // Error handling
    {"try", TK_TRY}, {"catch", TK_CATCH}, {"finally", TK_FINALLY},
    {"throw", TK_THROW},
    
    // Functions
    {"func", TK_FUNC}, {"import", TK_IMPORT}, {"export", TK_EXPORT},
    {"from", TK_FROM}, {"class", TK_CLASS}, {"struct", TK_STRUCT},
    {"enum", TK_ENUM}, {"interface", TK_INTERFACE}, {"typedef", TK_TYPEDEF},
    {"typelock", TK_TYPELOCK}, {"namespace", TK_NAMESPACE},
    
    // Types
    {"int", TK_TYPE_INT}, {"float", TK_TYPE_FLOAT}, {"string", TK_TYPE_STR},
    {"bool", TK_TYPE_BOOL}, {"char", TK_TYPE_CHAR}, {"void", TK_TYPE_VOID},
    {"any", TK_TYPE_ANY}, {"auto", TK_TYPE_AUTO}, {"unknown", TK_TYPE_UNKNOWN},
    {"netvar", TK_TYPE_NET}, {"clogvar", TK_TYPE_CLOG}, 
    {"dosvar", TK_TYPE_DOS}, {"selvar", TK_TYPE_SEL},
    {"array", TK_TYPE_ARRAY}, {"map", TK_TYPE_MAP}, {"func", TK_TYPE_FUNC},
    
    // Memory
    {"sizeof", TK_SIZEOF}, {"size", TK_SIZE}, {"siz", TK_SIZ},
    {"new", TK_NEW}, {"delete", TK_DELETE}, {"free", TK_FREE},
    
    // Debug
    {"db", TK_DB}, {"dbvar", TK_DBVAR}, {"printdb", TK_PRINT_DB},
    {"assert", TK_ASSERT},
    
    // I/O
    {"print", TK_PRINT}, {"weld", TK_WELD}, {"read", TK_READ},
    {"write", TK_WRITE},
    
    // New keywords
    {"pass", TK_PASS}, {"global", TK_GLOBAL}, {"lambda", TK_LAMBDA},
    {"bdd", TK_BDD}, {"def", TK_DEF}, {"type", TK_TYPE}, 
    {"raise", TK_RAISE}, {"with", TK_WITH}, {"learn", TK_LEARN},
    {"nonlocal", TK_NONLOCAL}, {"lock", TK_LOCK}, {"append", TK_APPEND},
    {"push", TK_PUSH}, {"pop", TK_POP},
    
    // JSON & Data
    {"json", TK_JSON}, {"yaml", TK_YAML}, {"xml", TK_XML},
    
    // Operators as keywords
    {"in", TK_IN}, {"is", TK_IS}, {"isnot", TK_ISNOT}, {"as", TK_AS_OP},
    {"of", TK_OF},
    
    // Special
    {"main", TK_MAIN}, {"this", TK_THIS}, {"self", TK_SELF},
    {"super", TK_SUPER}, {"static", TK_STATIC}, {"public", TK_PUBLIC},
    {"private", TK_PRIVATE}, {"protected", TK_PROTECTED},
    
    // Async
    {"async", TK_ASYNC}, {"await", TK_AWAIT},
    
    // Literals
    {"true", TK_TRUE}, {"false", TK_FALSE}, {"null", TK_NULL},
    {"undefined", TK_UNDEFINED}, {"nan", TK_NAN}, {"inf", TK_INF},
    
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
    // Ajouter ces types de nœuds dans l'énum NodeType
    NODE_FILE_OPEN,
     NODE_FILE_CLOSE,
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
    
    // Memory
    NODE_SIZEOF,
    NODE_NEW,
    NODE_DELETE,
    
    // Modules
    NODE_IMPORT,
    NODE_EXPORT,
    NODE_MODULE,
    
    // Debug
    NODE_DBVAR,
    NODE_ASSERT,
    
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
    NODE_EMPTY
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
        } imports;
        
        // Export
        struct {
            char* symbol;
            char* alias;
        } export;
        
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
        } case_stmt;
        
        // JSON/Data
        struct {
            char* data;
            char* format;
        } data_literal;
    } data;
    
    // Additional info
    int line;
    int column;
    TokenKind op_type;
} ASTNode;

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

#endif // COMMON_H
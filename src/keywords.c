// keywords.c - Définition des mots-clés SwiftFlow
#include "common.h"

const Keyword keywords[] = {
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

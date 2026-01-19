#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdarg.h>
#include "common.h"

// ======================================================
// [SECTION] LEXER STATE
// ======================================================
typedef struct {
    const char* start;
    const char* current;
    int line;
    int column;
    int start_column;
} Lexer;

static Lexer lexer;

// ======================================================
// [SECTION] LEXER UTILITIES
// ======================================================
void initLexer(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
    lexer.column = 1;
    lexer.start_column = 1;
}

static bool isAtEnd() { 
    return *lexer.current == '\0'; 
}

static char advance() {
    char c = *lexer.current;
    if (c == '\n') {
        lexer.line++;
        lexer.column = 1;
    } else {
        lexer.column++;
    }
    lexer.current++;
    return c;
}

static char peek() { 
    return *lexer.current; 
}

static char peekNext() { 
    if (isAtEnd()) return '\0';
    return lexer.current[1]; 
}

static bool match(char expected) {
    if (isAtEnd()) return false;
    if (*lexer.current != expected) return false;
    advance();
    return true;
}

// ======================================================
// [SECTION] TOKEN CREATION
// ======================================================
static Token makeToken(TokenKind kind) {
    Token token;
    token.kind = kind;
    token.start = lexer.start;
    token.length = (int)(lexer.current - lexer.start);
    token.line = lexer.line;
    token.column = lexer.start_column;
    
    // Initialize value
    token.value.int_val = 0;
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.kind = TK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    token.column = lexer.column;
    token.value.str_val = NULL;
    return token;
}

static Token makeStringToken(TokenKind kind, char* str) {
    Token token = makeToken(kind);
    token.value.str_val = str;
    return token;
}

static Token makeIntToken(int64_t value) {
    Token token = makeToken(TK_INT);
    token.value.int_val = value;
    return token;
}

static Token makeFloatToken(double value) {
    Token token = makeToken(TK_FLOAT);
    token.value.float_val = value;
    return token;
}

static Token makeBoolToken(bool value) {
    Token token = makeToken(value ? TK_TRUE : TK_FALSE);
    token.value.bool_val = value;
    return token;
}

// ======================================================
// [SECTION] SKIP WHITESPACE & COMMENTS
// ======================================================
static void skipWhitespace() {
    while (true) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                break;
            case '\n':
                lexer.line++;
                lexer.column = 1;
                advance();
                break;
            case '#': // Commentaire avec #
                while (peek() != '\n' && !isAtEnd()) advance();
                break;
            case '/':
                if (peekNext() == '/') { // Commentaire //
                    while (peek() != '\n' && !isAtEnd()) advance();
                } else if (peekNext() == '*') { // Commentaire /* */
                    advance(); // Skip '/'
                    advance(); // Skip '*'
                    while (!(peek() == '*' && peekNext() == '/') && !isAtEnd()) {
                        if (peek() == '\n') {
                            lexer.line++;
                            lexer.column = 1;
                        }
                        advance();
                    }
                    if (!isAtEnd()) {
                        advance(); // Skip '*'
                        advance(); // Skip '/'
                    }
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

// ======================================================
// [SECTION] STRING LEXING
// ======================================================
static Token string(char quote_char) {
    // Skip opening quote (already consumed)
    
    while (peek() != quote_char && !isAtEnd()) {
        if (peek() == '\n') {
            lexer.line++;
            lexer.column = 1;
        }
        if (peek() == '\\') { // Handle escape sequences
            advance();
            switch (peek()) {
                case 'n': case 't': case 'r': case '\\': 
                case '"': case '\'': case '0': case 'b': case 'f':
                case 'v': case 'x': case 'u':
                    advance();
                    break;
                default:
                    advance(); // Just skip unknown escape
                    break;
            }
        } else {
            advance();
        }
    }
    
    if (isAtEnd()) {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Unterminated string (started with '%c')", quote_char);
        return errorToken(error_msg);
    }
    
    // Skip closing quote
    advance();
    
    // Extract string without quotes
    int length = (int)(lexer.current - lexer.start - 2); // -2 for quotes
    char* str = malloc(length + 1);
    if (str) {
        // Copy string content (skip quotes)
        const char* src = lexer.start + 1;
        int dest_idx = 0;
        
        for (int i = 0; i < length; i++) {
            if (src[i] == '\\') {
                i++; // Skip backslash
                if (i < length) {
                    switch (src[i]) {
                        case 'n': str[dest_idx++] = '\n'; break;
                        case 't': str[dest_idx++] = '\t'; break;
                        case 'r': str[dest_idx++] = '\r'; break;
                        case '\\': str[dest_idx++] = '\\'; break;
                        case '"': str[dest_idx++] = '"'; break;
                        case '\'': str[dest_idx++] = '\''; break;
                        case '0': str[dest_idx++] = '\0'; break;
                        case 'b': str[dest_idx++] = '\b'; break;
                        case 'f': str[dest_idx++] = '\f'; break;
                        case 'v': str[dest_idx++] = '\v'; break;
                        default: str[dest_idx++] = src[i]; break;
                    }
                }
            } else {
                str[dest_idx++] = src[i];
            }
        }
        str[dest_idx] = '\0';
    }
    
    return makeStringToken(TK_STRING, str);
}

// ======================================================
// [SECTION] NUMBER LEXING
// ======================================================
static Token number() {
    bool is_float = false;
    bool is_hex = false;
    bool is_binary = false;
    bool is_octal = false;
    
    // Check for hex (0x) or binary (0b) or octal (0o)
    if (peek() == '0') {
        char next = peekNext();
        if (next == 'x' || next == 'X') {
            is_hex = true;
            advance(); // Skip '0'
            advance(); // Skip 'x' or 'X'
        } else if (next == 'b' || next == 'B') {
            is_binary = true;
            advance(); // Skip '0'
            advance(); // Skip 'b' or 'B'
        } else if (next == 'o' || next == 'O') {
            is_octal = true;
            advance(); // Skip '0'
            advance(); // Skip 'o' or 'O'
        }
    }
    
    if (is_hex) {
        // Parse hexadecimal
        while (isxdigit(peek())) advance();
        
        if (peek() == '.') {
            is_float = true;
            advance(); // Consume '.'
            while (isxdigit(peek())) advance();
        }
    } else if (is_binary) {
        // Parse binary
        while (peek() == '0' || peek() == '1') advance();
    } else if (is_octal) {
        // Parse octal
        while (peek() >= '0' && peek() <= '7') advance();
    } else {
        // Parse decimal
        while (isdigit(peek())) advance();
        
        // Decimal part
        if (peek() == '.' && isdigit(peekNext())) {
            is_float = true;
            advance(); // Consume '.'
            while (isdigit(peek())) advance();
        }
        
        // Scientific notation
        if (peek() == 'e' || peek() == 'E') {
            is_float = true;
            advance(); // Consume 'e' or 'E'
            if (peek() == '+' || peek() == '-') advance();
            while (isdigit(peek())) advance();
        }
    }
    
    // Parse the number
    int length = (int)(lexer.current - lexer.start);
    char* num_str = malloc(length + 1);
    if (num_str) {
        strncpy(num_str, lexer.start, length);
        num_str[length] = '\0';
        
        if (is_float) {
            double value = atof(num_str);
            free(num_str);
            return makeFloatToken(value);
        } else {
            // Determine base
            int base = 10;
            if (is_hex) base = 16;
            else if (is_binary) base = 2;
            else if (is_octal) base = 8;
            
            int64_t value = strtoll(num_str, NULL, base);
            free(num_str);
            return makeIntToken(value);
        }
    }
    
    return errorToken("Failed to parse number");
}

// ======================================================
// [SECTION] IDENTIFIER & KEYWORD LEXING
// ======================================================
static bool isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool isAlphaNumeric(char c) {
    return isAlpha(c) || isdigit(c);
}

static Token identifier() {
    while (isAlphaNumeric(peek()) || peek() == '.' || peek() == '$' || peek() == '@') {
        advance();
    }
    
    int length = (int)(lexer.current - lexer.start);
    char* text = malloc(length + 1);
    if (text) {
        strncpy(text, lexer.start, length);
        text[length] = '\0';
    }
    
    // Check for keywords
    if (text) {
        for (int i = 0; keywords[i].keyword != NULL; i++) {
            if (strcmp(text, keywords[i].keyword) == 0) {
                free(text);
                return makeToken(keywords[i].kind);
            }
        }
        
        // Check for special multi-word keywords
        if (strcmp(text, "print") == 0) {
            free(text);
            return makeToken(TK_PRINT);
        }
        
        // If not a keyword, it's an identifier
        Token token = makeToken(TK_IDENT);
        token.value.str_val = text;
        return token;
    }
    
    free(text);
    return errorToken("Failed to allocate memory for identifier");
}

// ======================================================
// [SECTION] OPERATOR LEXING
// ======================================================
static Token operatorLexer() {
    char c = peek();
    char error_msg[32]; // Déclaré en haut de la fonction
    
    // Multi-character operators
    switch (c) {
        case '=':
            advance();
            if (match('=')) {
                if (match('=')) return makeToken(TK_SPACESHIP); // ===
                if (match('>')) return makeToken(TK_RDARROW); // ==>
                return makeToken(TK_EQ); // ==
            }
            if (match('>')) return makeToken(TK_DARROW); // =>
            return makeToken(TK_ASSIGN); // =
            
        case '!':
            advance();
            if (match('=')) {
                if (match('=')) return makeToken(TK_NEQ); // !==
                return makeToken(TK_NEQ); // !=
            }
            return makeToken(TK_NOT); // !
            
        case '<':
            advance();
            if (match('=')) {
                if (match('=')) return makeToken(TK_LDARROW); // <==
                return makeToken(TK_LTE); // <=
            }
            if (match('<')) return makeToken(TK_SHL); // <<
            return makeToken(TK_LT); // <
            
        case '>':
            advance();
            if (match('=')) {
                if (match('=')) return makeToken(TK_RDARROW); // ==>
                return makeToken(TK_GTE); // >=
            }
            if (match('>')) return makeToken(TK_SHR); // >>
            return makeToken(TK_GT); // >
            
        case '&':
            advance();
            if (match('&')) return makeToken(TK_AND); // &&
            return makeToken(TK_BIT_AND); // &
            
        case '|':
            advance();
            if (match('|')) return makeToken(TK_OR); // ||
            return makeToken(TK_BIT_OR); // |
            
        case '+':
            advance();
            if (match('=')) return makeToken(TK_PLUS_ASSIGN); // +=
            if (match('+')) return makeToken(TK_PLUS); // ++ (treated as plus)
            return makeToken(TK_PLUS); // +
            
        case '-':
            advance();
            if (match('=')) return makeToken(TK_MINUS_ASSIGN); // -=
            if (match('-')) return makeToken(TK_MINUS); // -- (treated as minus)
            if (match('>')) return makeToken(TK_RARROW); // ->
            return makeToken(TK_MINUS); // -
            
        case '*':
            advance();
            if (match('=')) return makeToken(TK_MULT_ASSIGN); // *=
            if (match('*')) return makeToken(TK_POW); // **
            return makeToken(TK_MULT); // *
            
        case '/':
            advance();
            if (match('=')) return makeToken(TK_DIV_ASSIGN); // /=
            return makeToken(TK_DIV); // /
            
        case '%':
            advance();
            if (match('=')) return makeToken(TK_MOD_ASSIGN); // %=
            return makeToken(TK_MOD); // %
            
        case '^':
            advance();
            if (match('=')) return makeToken(TK_BIT_XOR); // ^= (treated as xor)
            return makeToken(TK_BIT_XOR); // ^
            
        case '~':
            advance();
            return makeToken(TK_BIT_NOT); // ~
            
        case '.':
            advance();
            if (match('.')) {
                if (match('.')) return makeToken(TK_ELLIPSIS); // ...
                return makeToken(TK_RANGE); // .. (treated as range)
            }
            return makeToken(TK_PERIOD); // .
            
        case '?':
            advance();
            return makeToken(TK_QUESTION); // ?
            
        case ':':
            advance();
            if (match(':')) return makeToken(TK_SCOPE); // ::
            return makeToken(TK_COLON); // :
            
        case '@':
            advance();
            return makeToken(TK_AT); // @
            
        case '$':
            advance();
            return makeToken(TK_DOLLAR); // $
            
        case '#':
            advance();
            return makeToken(TK_HASH); // #
            
        case ';':
            advance();
            return makeToken(TK_SEMICOLON); // ;
            
        case ',':
            advance();
            return makeToken(TK_COMMA); // ,
            
        case '(':
            advance();
            return makeToken(TK_LPAREN); // (
            
        case ')':
            advance();
            return makeToken(TK_RPAREN); // )
            
        case '{':
            advance();
            return makeToken(TK_LBRACE); // {
            
        case '}':
            advance();
            return makeToken(TK_RBRACE); // }
            
        case '[':
            advance();
            return makeToken(TK_LBRACKET); // [
            
        case ']':
            advance();
            return makeToken(TK_RBRACKET); // ]
            
        default:
            // Unknown character
            advance();
            snprintf(error_msg, sizeof(error_msg), "Unexpected character: '%c'", c);
            return errorToken(error_msg);
    }
}

// ======================================================
// [SECTION] MAIN LEXER FUNCTION
// ======================================================
Token scanToken() {
    skipWhitespace();
    lexer.start = lexer.current;
    lexer.start_column = lexer.column;
    
    if (isAtEnd()) return makeToken(TK_EOF);
    
    char c = peek();
    
    // Identifiers and keywords
    if (isAlpha(c) || c == '_' || c == '$' || c == '@') {
        return identifier();
    }
    
    // Numbers
    if (isdigit(c)) {
        return number();
    }
    
    // Strings
    if (c == '"' || c == '\'') {
        advance(); // Skip quote
        return string(c);
    }
    
    // Operators and punctuation
    return operatorLexer();
}

// ======================================================
// [SECTION] HELPER FUNCTIONS FOR DEBUGGING
// ======================================================
const char* tokenKindToString(TokenKind kind) {
    switch (kind) {
        case TK_INT: return "TK_INT";
        case TK_FLOAT: return "TK_FLOAT";
        case TK_STRING: return "TK_STRING";
        case TK_TRUE: return "TK_TRUE";
        case TK_FALSE: return "TK_FALSE";
        case TK_NULL: return "TK_NULL";
        case TK_UNDEFINED: return "TK_UNDEFINED";
        
        case TK_IDENT: return "TK_IDENT";
        
        // Operators
        case TK_PLUS: return "TK_PLUS";
        case TK_MINUS: return "TK_MINUS";
        case TK_MULT: return "TK_MULT";
        case TK_DIV: return "TK_DIV";
        case TK_MOD: return "TK_MOD";
        case TK_POW: return "TK_POW";
        case TK_ROOT: return "TK_ROOT";
        case TK_CONCAT: return "TK_CONCAT";
        
        // Assignment
        case TK_ASSIGN: return "TK_ASSIGN";
        case TK_PLUS_ASSIGN: return "TK_PLUS_ASSIGN";
        case TK_MINUS_ASSIGN: return "TK_MINUS_ASSIGN";
        case TK_MULT_ASSIGN: return "TK_MULT_ASSIGN";
        case TK_DIV_ASSIGN: return "TK_DIV_ASSIGN";
        case TK_MOD_ASSIGN: return "TK_MOD_ASSIGN";
        
        // Comparison
        case TK_EQ: return "TK_EQ";
        case TK_NEQ: return "TK_NEQ";
        case TK_GT: return "TK_GT";
        case TK_LT: return "TK_LT";
        case TK_GTE: return "TK_GTE";
        case TK_LTE: return "TK_LTE";
        case TK_AND: return "TK_AND";
        case TK_OR: return "TK_OR";
        case TK_NOT: return "TK_NOT";
        case TK_SPACESHIP: return "TK_SPACESHIP";
        
        // Bitwise
        case TK_BIT_AND: return "TK_BIT_AND";
        case TK_BIT_OR: return "TK_BIT_OR";
        case TK_BIT_XOR: return "TK_BIT_XOR";
        case TK_BIT_NOT: return "TK_BIT_NOT";
        case TK_SHL: return "TK_SHL";
        case TK_SHR: return "TK_SHR";
        
        // Arrow
        case TK_LARROW: return "TK_LARROW";
        case TK_RARROW: return "TK_RARROW";
        case TK_DARROW: return "TK_DARROW";
        case TK_LDARROW: return "TK_LDARROW";
        case TK_RDARROW: return "TK_RDARROW";
        
        // Range
        case TK_ELLIPSIS: return "TK_ELLIPSIS";
        case TK_RANGE: return "TK_RANGE";
        case TK_SCOPE: return "TK_SCOPE";
        
        // Punctuation
        case TK_LPAREN: return "TK_LPAREN";
        case TK_RPAREN: return "TK_RPAREN";
        case TK_LBRACE: return "TK_LBRACE";
        case TK_RBRACE: return "TK_RBRACE";
        case TK_LBRACKET: return "TK_LBRACKET";
        case TK_RBRACKET: return "TK_RBRACKET";
        case TK_COMMA: return "TK_COMMA";
        case TK_SEMICOLON: return "TK_SEMICOLON";
        case TK_COLON: return "TK_COLON";
        case TK_PERIOD: return "TK_PERIOD";
        case TK_AT: return "TK_AT";
        case TK_HASH: return "TK_HASH";
        case TK_DOLLAR: return "TK_DOLLAR";
        case TK_QUESTION: return "TK_QUESTION";
        case TK_PIPE: return "TK_PIPE";
        case TK_AMPERSAND: return "TK_AMPERSAND";
        case TK_TILDE: return "TK_TILDE";
        case TK_CARET: return "TK_CARET";
        
        // Keywords
        case TK_VAR: return "TK_VAR";
        case TK_LET: return "TK_LET";
        case TK_CONST: return "TK_CONST";
        case TK_NET: return "TK_NET";
        case TK_CLOG: return "TK_CLOG";
        case TK_DOS: return "TK_DOS";
        case TK_SEL: return "TK_SEL";
        
        // Control flow
        case TK_IF: return "TK_IF";
        case TK_ELSE: return "TK_ELSE";
        case TK_ELIF: return "TK_ELIF";
        case TK_WHILE: return "TK_WHILE";
        case TK_FOR: return "TK_FOR";
        case TK_DO: return "TK_DO";
        case TK_SWITCH: return "TK_SWITCH";
        case TK_CASE: return "TK_CASE";
        case TK_DEFAULT: return "TK_DEFAULT";
        case TK_BREAK: return "TK_BREAK";
        case TK_CONTINUE: return "TK_CONTINUE";
        case TK_RETURN: return "TK_RETURN";
        
        // Functions & Modules
        case TK_FUNC: return "TK_FUNC";
        case TK_IMPORT: return "TK_IMPORT";
        case TK_EXPORT: return "TK_EXPORT";
        case TK_FROM: return "TK_FROM";
        case TK_CLASS: return "TK_CLASS";
        case TK_STRUCT: return "TK_STRUCT";
        case TK_ENUM: return "TK_ENUM";
        case TK_TYPEDEF: return "TK_TYPEDEF";
        case TK_TYPELOCK: return "TK_TYPELOCK";
        case TK_NAMESPACE: return "TK_NAMESPACE";
        case TK_PUBLIC: return "TK_PUBLIC";
        case TK_PRIVATE: return "TK_PRIVATE";
        case TK_PROTECTED: return "TK_PROTECTED";
        
        // Types
        case TK_TYPE_INT: return "TK_TYPE_INT";
        case TK_TYPE_FLOAT: return "TK_TYPE_FLOAT";
        case TK_TYPE_STR: return "TK_TYPE_STR";
        case TK_TYPE_BOOL: return "TK_TYPE_BOOL";
        case TK_TYPE_CHAR: return "TK_TYPE_CHAR";
        case TK_TYPE_VOID: return "TK_TYPE_VOID";
        case TK_TYPE_ANY: return "TK_TYPE_ANY";
        case TK_TYPE_AUTO: return "TK_TYPE_AUTO";
        case TK_TYPE_NET: return "TK_TYPE_NET";
        case TK_TYPE_CLOG: return "TK_TYPE_CLOG";
        case TK_TYPE_DOS: return "TK_TYPE_DOS";
        case TK_TYPE_SEL: return "TK_TYPE_SEL";
        case TK_TYPE_MODULE: return "TK_TYPE_MODULE";
        
        // Memory
        case TK_SIZEOF: return "TK_SIZEOF";
        case TK_SIZE: return "TK_SIZE";
        case TK_SIZ: return "TK_SIZ";
        case TK_NEW: return "TK_NEW";
        case TK_DELETE: return "TK_DELETE";
        case TK_FREE: return "TK_FREE";
        case TK_ALLOC: return "TK_ALLOC";
        case TK_REALLOC: return "TK_REALLOC";
        
        // Debug
        case TK_DB: return "TK_DB";
        case TK_DBVAR: return "TK_DBVAR";
        case TK_PRINT_DB: return "TK_PRINT_DB";
        case TK_TRACE: return "TK_TRACE";
        
        // I/O
        case TK_PRINT: return "TK_PRINT";
        case TK_INPUT: return "TK_INPUT";
        case TK_READ: return "TK_READ";
        case TK_WRITE: return "TK_WRITE";
        
        // Special
        case TK_MAIN: return "TK_MAIN";
        case TK_THIS: return "TK_THIS";
        case TK_SUPER: return "TK_SUPER";
        case TK_SELF: return "TK_SELF";
        case TK_INIT: return "TK_INIT";
        case TK_DEINIT: return "TK_DEINIT";
        case TK_CONSTRUCTOR: return "TK_CONSTRUCTOR";
        case TK_DESTRUCTOR: return "TK_DESTRUCTOR";
        
        // End markers
        case TK_EOF: return "TK_EOF";
        case TK_ERROR: return "TK_ERROR";
        case TK_UNKNOWN: return "TK_UNKNOWN";
        
        default: return "TK_UNKNOWN";
    }
}

void printToken(Token token) {
    printf("[TOKEN] Line %d, Col %d: %s '", 
           token.line, token.column, tokenKindToString(token.kind));
    
    if (token.kind == TK_STRING && token.value.str_val) {
        printf("%s", token.value.str_val);
    } else if (token.kind == TK_INT) {
        printf("%lld", token.value.int_val);
    } else if (token.kind == TK_FLOAT) {
        printf("%f", token.value.float_val);
    } else if (token.kind == TK_TRUE || token.kind == TK_FALSE) {
        printf("%s", token.value.bool_val ? "true" : "false");
    } else {
        for (int i = 0; i < token.length; i++) {
            putchar(token.start[i]);
        }
    }
    
    printf("'\n");
}

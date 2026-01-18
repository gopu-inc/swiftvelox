#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "common.h"

// ======================================================
// [SECTION] LEXER STATE
// ======================================================
typedef struct {
    const char* start;
    const char* current;
    int line;
} Lexer;

static Lexer lexer;

// ======================================================
// [SECTION] LEXER UTILITIES
// ======================================================
void initLexer(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
}

static bool isAtEnd() { 
    return *lexer.current == '\0'; 
}

static char advance() { 
    lexer.current++;
    return lexer.current[-1];
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
    lexer.current++;
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
    return token;
}

static Token errorToken(const char* message) {
    Token token;
    token.kind = TK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
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
                advance();
                break;
            case '#': // Commentaire avec #
                while (peek() != '\n' && !isAtEnd()) advance();
                break;
            case '/':
                if (peekNext() == '/') { // Commentaire //
                    while (peek() != '\n' && !isAtEnd()) advance();
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
static Token string() {
    // Skip opening quote
    advance();
    
    while (peek() != '"' && !isAtEnd()) {
        if (peek() == '\n') lexer.line++;
        if (peek() == '\\') { // Handle escape sequences
            advance();
            switch (peek()) {
                case 'n': case 't': case 'r': case '\\': case '"':
                    advance();
                    break;
            }
        } else {
            advance();
        }
    }
    
    if (isAtEnd()) {
        return errorToken("Unterminated string.");
    }
    
    // Skip closing quote
    advance();
    
    // Extract string without quotes
    int length = (int)(lexer.current - lexer.start - 2); // -2 for quotes
    char* str = malloc(length + 1);
    if (str) {
        // Copy string content (skip quotes)
        const char* src = lexer.start + 1;
        char* dest = str;
        for (int i = 0; i < length; i++) {
            if (src[i] == '\\') {
                i++; // Skip backslash
                switch (src[i]) {
                    case 'n': *dest++ = '\n'; break;
                    case 't': *dest++ = '\t'; break;
                    case 'r': *dest++ = '\r'; break;
                    case '\\': *dest++ = '\\'; break;
                    case '"': *dest++ = '"'; break;
                    default: *dest++ = src[i]; break;
                }
            } else {
                *dest++ = src[i];
            }
        }
        *dest = '\0';
    }
    
    Token token = makeToken(TK_STRING);
    token.value.str_val = str;
    return token;
}

// ======================================================
// [SECTION] NUMBER LEXING
// ======================================================
static Token number() {
    bool is_float = false;
    
    // Integer part
    while (isdigit(peek())) advance();
    
    // Decimal part
    if (peek() == '.' && isdigit(peekNext())) {
        is_float = true;
        advance(); // Consume '.'
        while (isdigit(peek())) advance();
    }
    
    if (is_float) {
        // Parse as float
        char* num_str = malloc(lexer.current - lexer.start + 1);
        if (num_str) {
            strncpy(num_str, lexer.start, lexer.current - lexer.start);
            num_str[lexer.current - lexer.start] = '\0';
            
            Token token = makeToken(TK_FLOAT);
            token.value.float_val = atof(num_str);
            free(num_str);
            return token;
        }
    } else {
        // Parse as integer
        char* num_str = malloc(lexer.current - lexer.start + 1);
        if (num_str) {
            strncpy(num_str, lexer.start, lexer.current - lexer.start);
            num_str[lexer.current - lexer.start] = '\0';
            
            Token token = makeToken(TK_INT);
            token.value.int_val = atoll(num_str);
            free(num_str);
            return token;
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
    while (isAlphaNumeric(peek()) || peek() == '.') advance();
    
    int length = (int)(lexer.current - lexer.start);
    char* text = malloc(length + 1);
    if (text) {
        strncpy(text, lexer.start, length);
        text[length] = '\0';
    }
    
    // Check for keywords
    if (text) {
        if (strcmp(text, "var") == 0) {
            free(text);
            return makeToken(TK_VAR);
        }
        if (strcmp(text, "print") == 0) {
            free(text);
            return makeToken(TK_PRINT);
        }
        if (strcmp(text, "if") == 0) {
            free(text);
            return makeToken(TK_IF);
        }
        if (strcmp(text, "else") == 0) {
            free(text);
            return makeToken(TK_ELSE);
        }
        if (strcmp(text, "while") == 0) {
            free(text);
            return makeToken(TK_WHILE);
        }
        if (strcmp(text, "for") == 0) {
            free(text);
            return makeToken(TK_FOR);
        }
        if (strcmp(text, "func") == 0) {
            free(text);
            return makeToken(TK_FUNC);
        }
        if (strcmp(text, "return") == 0) {
            free(text);
            return makeToken(TK_RETURN);
        }
        if (strcmp(text, "import") == 0) {
            free(text);
            return makeToken(TK_IMPORT);
        }
        if (strcmp(text, "true") == 0) {
            free(text);
            return makeToken(TK_TRUE);
        }
        if (strcmp(text, "false") == 0) {
            free(text);
            return makeToken(TK_FALSE);
        }
        if (strcmp(text, "int") == 0) {
            free(text);
            return makeToken(TK_TYPE_INT);
        }
        if (strcmp(text, "float") == 0) {
            free(text);
            return makeToken(TK_TYPE_FLOAT);
        }
        if (strcmp(text, "string") == 0) {
            free(text);
            return makeToken(TK_TYPE_STR);
        }
        if (strcmp(text, "bool") == 0) {
            free(text);
            return makeToken(TK_TYPE_BOOL);
        }
        if (strcmp(text, "from") == 0) {
            free(text);
            // "from" est traité comme un identifiant spécial
            Token token = makeToken(TK_IDENT);
            token.value.str_val = str_copy("from");
            return token;
        }
        
        // Si not a keyword, it's an identifier
        Token token = makeToken(TK_IDENT);
        token.value.str_val = text;
        return token;
    }
    
    free(text);
    return errorToken("Failed to allocate memory for identifier");
}

// ======================================================
// [SECTION] MAIN LEXER FUNCTION
// ======================================================
Token scanToken() {
    skipWhitespace();
    lexer.start = lexer.current;
    
    if (isAtEnd()) return makeToken(TK_EOF);
    
    char c = advance();
    
    // Single character tokens
    switch (c) {
        case '(': return makeToken(TK_LPAREN);
        case ')': return makeToken(TK_RPAREN);
        case '{': return makeToken(TK_LBRACE);
        case '}': return makeToken(TK_RBRACE);
        case ',': return makeToken(TK_COMMA);
        case ';': return makeToken(TK_SEMICOLON);
        case ':': return makeToken(TK_COLON);
        case '.': return makeToken(TK_PERIOD);
        
        // Operators
        case '+': return makeToken(TK_PLUS);
        case '-': return makeToken(TK_MINUS);
        case '*': return makeToken(TK_MULT);
        case '/': return makeToken(TK_DIV);
        case '%': return makeToken(TK_MOD);
        
        // Comparison and assignment
        case '=':
            if (match('=')) return makeToken(TK_EQ);
            return makeToken(TK_ASSIGN);
        case '!':
            if (match('=')) return makeToken(TK_NEQ);
            return makeToken(TK_NOT);
        case '<':
            if (match('=')) return makeToken(TK_LTE);
            return makeToken(TK_LT);
        case '>':
            if (match('=')) return makeToken(TK_GTE);
            return makeToken(TK_GT);
        
        // Logical operators
        case '&':
            if (match('&')) return makeToken(TK_AND);
            break;
        case '|':
            if (match('|')) return makeToken(TK_OR);
            break;
        
        // String literal
        case '"': return string();
    }
    
    // Numbers
    if (isdigit(c)) return number();
    
    // Identifiers and keywords
    if (isAlpha(c)) return identifier();
    
    // Unknown character
    char error_msg[32];
    snprintf(error_msg, sizeof(error_msg), "Unexpected character: '%c'", c);
    return errorToken(error_msg);
}

// ======================================================
// [SECTION] HELPER FUNCTIONS FOR DEBUGGING
// ======================================================
const char* tokenKindToString(TokenKind kind) {
    switch (kind) {
        case TK_INT: return "TK_INT";
        case TK_FLOAT: return "TK_FLOAT";
        case TK_STRING: return "TK_STRING";
        case TK_IDENT: return "TK_IDENT";
        case TK_PLUS: return "TK_PLUS";
        case TK_MINUS: return "TK_MINUS";
        case TK_MULT: return "TK_MULT";
        case TK_DIV: return "TK_DIV";
        case TK_MOD: return "TK_MOD";
        case TK_EQ: return "TK_EQ";
        case TK_NEQ: return "TK_NEQ";
        case TK_ASSIGN: return "TK_ASSIGN";
        case TK_VAR: return "TK_VAR";
        case TK_PRINT: return "TK_PRINT";
        case TK_IF: return "TK_IF";
        case TK_ELSE: return "TK_ELSE";
        case TK_WHILE: return "TK_WHILE";
        case TK_FOR: return "TK_FOR";
        case TK_FUNC: return "TK_FUNC";
        case TK_RETURN: return "TK_RETURN";
        case TK_IMPORT: return "TK_IMPORT";
        case TK_TRUE: return "TK_TRUE";
        case TK_FALSE: return "TK_FALSE";
        case TK_LPAREN: return "TK_LPAREN";
        case TK_RPAREN: return "TK_RPAREN";
        case TK_LBRACE: return "TK_LBRACE";
        case TK_RBRACE: return "TK_RBRACE";
        case TK_COMMA: return "TK_COMMA";
        case TK_SEMICOLON: return "TK_SEMICOLON";
        case TK_COLON: return "TK_COLON";
        case TK_PERIOD: return "TK_PERIOD";
        case TK_EOF: return "TK_EOF";
        case TK_ERROR: return "TK_ERROR";
        case TK_AND: return "TK_AND";
        case TK_OR: return "TK_OR";
        case TK_NOT: return "TK_NOT";
        case TK_GT: return "TK_GT";
        case TK_LT: return "TK_LT";
        case TK_GTE: return "TK_GTE";
        case TK_LTE: return "TK_LTE";
        case TK_TYPE_INT: return "TK_TYPE_INT";
        case TK_TYPE_FLOAT: return "TK_TYPE_FLOAT";
        case TK_TYPE_STR: return "TK_TYPE_STR";
        case TK_TYPE_BOOL: return "TK_TYPE_BOOL";
        default: return "TK_UNKNOWN";
    }
}

void printToken(Token token) {
    printf("[TOKEN] Line %d: %s '", token.line, tokenKindToString(token.kind));
    
    if (token.kind == TK_STRING && token.value.str_val) {
        printf("%s", token.value.str_val);
    } else if (token.kind == TK_INT) {
        printf("%lld", token.value.int_val);
    } else if (token.kind == TK_FLOAT) {
        printf("%f", token.value.float_val);
    } else {
        for (int i = 0; i < token.length; i++) {
            putchar(token.start[i]);
        }
    }
    
    printf("'\n");
}

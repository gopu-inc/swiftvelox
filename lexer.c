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
    char error_msg[32];
    
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

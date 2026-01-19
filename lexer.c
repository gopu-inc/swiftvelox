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
    Error* error;
} Lexer;

static Lexer lexer;
static Token current_token;
static Token previous_token;
static Token peek_token;
static bool has_peek = false;

// ======================================================
// [SECTION] LEXER UTILITIES
// ======================================================
void init_lexer(const char* source) {
    lexer.start = source;
    lexer.current = source;
    lexer.line = 1;
    lexer.column = 1;
    lexer.start_column = 1;
    lexer.error = NULL;
    has_peek = false;
}

static bool is_at_end() { 
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

static char peek_next() { 
    if (is_at_end()) return '\0';
    return lexer.current[1]; 
}

static bool match(char expected) {
    if (is_at_end()) return false;
    if (*lexer.current != expected) return false;
    advance();
    return true;
}

static bool is_alpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c == '$';
}

static bool is_alpha_numeric(char c) {
    return is_alpha(c) || isdigit(c);
}

// ======================================================
// [SECTION] TOKEN CREATION
// ======================================================
static Token make_token(TokenKind kind) {
    Token token;
    token.kind = kind;
    token.start = lexer.start;
    token.length = (int)(lexer.current - lexer.start);
    token.line = lexer.line;
    token.column = lexer.start_column;
    
    // Initialiser les valeurs
    token.value.int_val = 0;
    token.value.str_val = NULL;
    return token;
}

static Token error_token(const char* message) {
    Token token;
    token.kind = TK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer.line;
    token.column = lexer.column;
    token.value.str_val = NULL;
    
    if (lexer.error) {
        set_error(lexer.error, lexer.line, lexer.column, NULL, "Lexer error: %s", message);
    }
    
    return token;
}

static Token make_string_token(char* str) {
    Token token = make_token(TK_STRING);
    token.value.str_val = str;
    return token;
}

static Token make_char_token(char c) {
    Token token = make_token(TK_CHAR);
    token.value.char_val = c;
    return token;
}

static Token make_int_token(int64_t value) {
    Token token = make_token(TK_INT);
    token.value.int_val = value;
    return token;
}

static Token make_float_token(double value) {
    Token token = make_token(TK_FLOAT);
    token.value.float_val = value;
    return token;
}

// ======================================================
// [SECTION] SKIP WHITESPACE & COMMENTS
// ======================================================
static void skip_whitespace() {
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
            case '#':
                // Commentaire ligne
                while (peek() != '\n' && !is_at_end()) advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    // Commentaire //
                    while (peek() != '\n' && !is_at_end()) advance();
                } else if (peek_next() == '*') {
                    // Commentaire /* */
                    advance(); // Skip '/'
                    advance(); // Skip '*'
                    while (!(peek() == '*' && peek_next() == '/') && !is_at_end()) {
                        if (peek() == '\n') {
                            lexer.line++;
                            lexer.column = 1;
                        }
                        advance();
                    }
                    if (!is_at_end()) {
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
static Token string_literal(char quote_char) {
    // Skip opening quote (already consumed)
    
    while (peek() != quote_char && !is_at_end()) {
        if (peek() == '\n') {
            lexer.line++;
            lexer.column = 1;
        }
        if (peek() == '\\') { // Handle escape sequences
            advance();
            switch (peek()) {
                case 'n': advance(); break;
                case 't': advance(); break;
                case 'r': advance(); break;
                case '\\': advance(); break;
                case '"': advance(); break;
                case '\'': advance(); break;
                case '0': advance(); break;
                case 'b': advance(); break;
                case 'f': advance(); break;
                case 'v': advance(); break;
                case 'x': advance(); break;
                case 'u': advance(); break;
                default: advance(); break;
            }
        } else {
            advance();
        }
    }
    
    if (is_at_end()) {
        char error_msg[64];
        snprintf(error_msg, sizeof(error_msg), "Unterminated string literal");
        return error_token(error_msg);
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
    
    return make_string_token(str);
}

static Token char_literal() {
    advance(); // Skip opening quote
    
    if (peek() == '\\') { // Escape sequence
        advance();
        char escaped_char = peek();
        advance();
        
        char actual_char;
        switch (escaped_char) {
            case 'n': actual_char = '\n'; break;
            case 't': actual_char = '\t'; break;
            case 'r': actual_char = '\r'; break;
            case '\\': actual_char = '\\'; break;
            case '\'': actual_char = '\''; break;
            case '0': actual_char = '\0'; break;
            default: actual_char = escaped_char; break;
        }
        
        if (peek() != '\'') {
            return error_token("Unterminated character literal");
        }
        advance();
        
        return make_char_token(actual_char);
    } else {
        char c = peek();
        advance();
        
        if (peek() != '\'') {
            return error_token("Unterminated character literal");
        }
        advance();
        
        return make_char_token(c);
    }
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
        char next = peek_next();
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
        if (peek() == '.' && isdigit(peek_next())) {
            is_float = true;
            advance(); // Consume '.'
            while (isdigit(peek())) advance();
        }
        
        // Exponent part
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
            return make_float_token(value);
        } else {
            // Determine base
            int base = 10;
            if (is_hex) base = 16;
            else if (is_binary) base = 2;
            else if (is_octal) base = 8;
            
            int64_t value = strtoll(num_str, NULL, base);
            free(num_str);
            return make_int_token(value);
        }
    }
    
    return error_token("Failed to parse number");
}

// ======================================================
// [SECTION] IDENTIFIER & KEYWORD LEXING
// ======================================================
static Token identifier() {
    while (is_alpha_numeric(peek()) || peek() == '.' || peek() == '@') {
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
                return make_token(keywords[i].kind);
            }
        }
        
        // If not a keyword, it's an identifier
        Token token = make_token(TK_IDENT);
        token.value.str_val = text;
        return token;
    }
    
    free(text);
    return error_token("Failed to allocate memory for identifier");
}

// ======================================================
// [SECTION] OPERATOR LEXING (COMPLET)
// ======================================================
static Token operator_lexer() {
    char c = peek();
    
    switch (c) {
        case '=':
            advance();
            if (match('=')) {
                if (match('=')) return make_token(TK_SPACESHIP);
                if (match('>')) return make_token(TK_RDARROW);
                return make_token(TK_EQ);
            }
            if (match('>')) return make_token(TK_DARROW);
            return make_token(TK_ASSIGN);
            
        case '!':
            advance();
            if (match('=')) {
                if (match('=')) return make_token(TK_NEQ);
                return make_token(TK_NEQ);
            }
            return make_token(TK_NOT);
            
        case '<':
            advance();
            if (match('=')) {
                if (match('=')) return make_token(TK_LDARROW);
                if (match('>')) return make_token(TK_SPACESHIP);
                return make_token(TK_LTE);
            }
            if (match('<')) {
                if (match('=')) return make_token(TK_SHL_ASSIGN);
                return make_token(TK_SHL);
            }
            return make_token(TK_LT);
            
        case '>':
            advance();
            if (match('=')) {
                if (match('=')) return make_token(TK_RDARROW);
                return make_token(TK_GTE);
            }
            if (match('>')) {
                if (match('=')) return make_token(TK_SHR_ASSIGN);
                return make_token(TK_SHR);
            }
            return make_token(TK_GT);
            
        case '&':
            advance();
            if (match('&')) return make_token(TK_AND);
            if (match('=')) return make_token(TK_AND_ASSIGN);
            return make_token(TK_BIT_AND);
            
        case '|':
            advance();
            if (match('|')) return make_token(TK_OR);
            if (match('=')) return make_token(TK_OR_ASSIGN);
            return make_token(TK_BIT_OR);
            
        case '+':
            advance();
            if (match('=')) return make_token(TK_PLUS_ASSIGN);
            if (match('+')) return make_token(TK_INC);
            return make_token(TK_PLUS);
            
        case '-':
            advance();
            if (match('=')) return make_token(TK_MINUS_ASSIGN);
            if (match('-')) return make_token(TK_DEC);
            if (match('>')) return make_token(TK_RARROW);
            return make_token(TK_MINUS);
            
        case '*':
            advance();
            if (match('=')) return make_token(TK_MULT_ASSIGN);
            if (match('*')) {
                if (match('=')) return make_token(TK_POW); // **= is pow assign
                return make_token(TK_POW);
            }
            return make_token(TK_MULT);
            
        case '/':
            advance();
            if (match('=')) return make_token(TK_DIV_ASSIGN);
            return make_token(TK_DIV);
            
        case '%':
            advance();
            if (match('=')) return make_token(TK_MOD_ASSIGN);
            return make_token(TK_MOD);
            
        case '^':
            advance();
            if (match('=')) return make_token(TK_XOR_ASSIGN);
            return make_token(TK_BIT_XOR);
            
        case '~':
            advance();
            return make_token(TK_BIT_NOT);
            
        case '.':
            advance();
            if (match('.')) {
                if (match('.')) return make_token(TK_SPREAD);
                return make_token(TK_RANGE);
            }
            if (match('?')) return make_token(TK_SAFE_NAV);
            return make_token(TK_PERIOD);
            
        case '?':
            advance();
            if (match('.')) return make_token(TK_SAFE_NAV);
            if (match('?')) return make_token(TK_NULL_COALESCE);
            return make_token(TK_QUESTION);
            
        case ':':
            advance();
            if (match(':')) return make_token(TK_SCOPE);
            return make_token(TK_COLON);
            
        case '@':
            advance();
            return make_token(TK_AT);
            
        case '$':
            advance();
            return make_token(TK_DOLLAR);
            
        case '#':
            advance();
            return make_token(TK_HASH);
            
        case '`':
            advance();
            return make_token(TK_BACKTICK);
            
        case ';':
            advance();
            return make_token(TK_SEMICOLON);
            
        case ',':
            advance();
            return make_token(TK_COMMA);
            
        case '(':
            advance();
            return make_token(TK_LPAREN);
            
        case ')':
            advance();
            return make_token(TK_RPAREN);
            
        case '{':
            advance();
            return make_token(TK_LBRACE);
            
        case '}':
            advance();
            return make_token(TK_RBRACE);
            
        case '[':
            advance();
            return make_token(TK_LBRACKET);
            
        case ']':
            advance();
            return make_token(TK_RBRACKET);
            
        default:
            advance();
            char error_msg[32];
            snprintf(error_msg, sizeof(error_msg), "Unexpected character: '%c'", c);
            return error_token(error_msg);
    }
}

// ======================================================
// [SECTION] MAIN LEXER FUNCTION
// ======================================================
Token scan_token() {
    skip_whitespace();
    lexer.start = lexer.current;
    lexer.start_column = lexer.column;
    
    if (is_at_end()) return make_token(TK_EOF);
    
    char c = peek();
    
    // Identifiers and keywords
    if (is_alpha(c)) {
        return identifier();
    }
    
    // Numbers
    if (isdigit(c)) {
        return number();
    }
    
    // Strings and characters
    if (c == '"' || c == '\'') {
        advance(); // Skip quote
        if (c == '\'' && peek() != '\'') {
            return char_literal();
        }
        return string_literal(c);
    }
    
    // Template strings
    if (c == '`') {
        advance();
        return string_literal('`');
    }
    
    // Operators and punctuation
    return operator_lexer();
}

Token peek_token() {
    if (!has_peek) {
        const char* saved_current = lexer.current;
        int saved_line = lexer.line;
        int saved_column = lexer.column;
        int saved_start_column = lexer.start_column;
        const char* saved_start = lexer.start;
        
        peek_token = scan_token();
        has_peek = true;
        
        lexer.current = saved_current;
        lexer.line = saved_line;
        lexer.column = saved_column;
        lexer.start_column = saved_start_column;
        lexer.start = saved_start;
    }
    return peek_token;
}

Token previous_token() {
    return previous_token;
}

bool is_at_end_wrapper() {
    return is_at_end();
}

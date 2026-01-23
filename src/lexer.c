#include "lexer.h"
#include "common.h"
#include <ctype.h>
#include <math.h>

void lexer_init(Lexer* lexer, const char* source, const char* filename) {
    lexer->source = source;
    lexer->start = source;
    lexer->current = source;
    lexer->line = 1;
    lexer->column = 1;
    lexer->filename = str_copy(filename);
    memset(&lexer->current_token, 0, sizeof(Token));
}

bool lexer_is_at_end(Lexer* lexer) {
    return *lexer->current == '\0';
}

char lexer_advance(Lexer* lexer) {
    lexer->current++;
    lexer->column++;
    return lexer->current[-1];
}

char lexer_peek(Lexer* lexer) {
    return *lexer->current;
}

char lexer_peek_next(Lexer* lexer) {
    if (lexer_is_at_end(lexer)) return '\0';
    return lexer->current[1];
}

bool lexer_match(Lexer* lexer, char expected) {
    if (lexer_is_at_end(lexer)) return false;
    if (*lexer->current != expected) return false;
    lexer_advance(lexer);
    return true;
}

void lexer_skip_whitespace(Lexer* lexer) {
    while (true) {
        char c = lexer_peek(lexer);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
                lexer_advance(lexer);
                break;
            case '\n':
                lexer->line++;
                lexer->column = 1;
                lexer_advance(lexer);
                break;
            case '#':  // Single line comment
                while (lexer_peek(lexer) != '\n' && !lexer_is_at_end(lexer)) {
                    lexer_advance(lexer);
                }
                break;
            case '/':
                if (lexer_peek_next(lexer) == '/') {
                    // Line comment
                    while (lexer_peek(lexer) != '\n' && !lexer_is_at_end(lexer)) {
                        lexer_advance(lexer);
                    }
                } else if (lexer_peek_next(lexer) == '*') {
                    // Block comment
                    lexer_advance(lexer); // Skip /
                    lexer_advance(lexer); // Skip *
                    while (!lexer_is_at_end(lexer)) {
                        if (lexer_peek(lexer) == '*' && lexer_peek_next(lexer) == '/') {
                            lexer_advance(lexer); // Skip *
                            lexer_advance(lexer); // Skip /
                            break;
                        }
                        if (lexer_peek(lexer) == '\n') {
                            lexer->line++;
                            lexer->column = 1;
                        }
                        lexer_advance(lexer);
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

Token lexer_make_token(Lexer* lexer, TokenKind kind) {
    Token token;
    token.kind = kind;
    token.start = lexer->start;
    token.length = (int)(lexer->current - lexer->start);
    token.line = lexer->line;
    token.column = lexer->column - token.length;
    return token;
}

Token lexer_error_token(Lexer* lexer, const char* message) {
    Token token;
    token.kind = TK_ERROR;
    token.start = message;
    token.length = (int)strlen(message);
    token.line = lexer->line;
    token.column = lexer->column;
    return token;
}

Token lexer_string(Lexer* lexer) {
    while (lexer_peek(lexer) != '"' && !lexer_is_at_end(lexer)) {
        if (lexer_peek(lexer) == '\n') {
            lexer->line++;
            lexer->column = 1;
        }
        lexer_advance(lexer);
    }
    
    if (lexer_is_at_end(lexer)) {
        return lexer_error_token(lexer, "Unterminated string");
    }
    
    lexer_advance(lexer); // Consume closing "
    
    Token token = lexer_make_token(lexer, TK_STRING);
    
    // Extract string value
    int length = token.length - 2; // Without quotes
    char* str = malloc(length + 1);
    if (str) {
        strncpy(str, token.start + 1, length);
        str[length] = '\0';
        
        // Handle escape sequences
        char* dest = str;
        const char* src = str;
        while (*src) {
            if (*src == '\\') {
                src++;
                switch (*src) {
                    case 'n': *dest++ = '\n'; break;
                    case 't': *dest++ = '\t'; break;
                    case 'r': *dest++ = '\r'; break;
                    case '\\': *dest++ = '\\'; break;
                    case '"': *dest++ = '"'; break;
                    default: *dest++ = '\\'; *dest++ = *src; break;
                }
                src++;
            } else {
                *dest++ = *src++;
            }
        }
        *dest = '\0';
        token.value.str_val = str;
    }
    
    return token;
}

Token lexer_number(Lexer* lexer) {
    bool is_float = false;
    
    // Integer part
    while (isdigit(lexer_peek(lexer))) {
        lexer_advance(lexer);
    }
    
    // Fraction part
    if (lexer_peek(lexer) == '.' && isdigit(lexer_peek_next(lexer))) {
        is_float = true;
        lexer_advance(lexer); // Consume .
        
        while (isdigit(lexer_peek(lexer))) {
            lexer_advance(lexer);
        }
    }
    
    // Exponent part
    if (lexer_peek(lexer) == 'e' || lexer_peek(lexer) == 'E') {
        is_float = true;
        lexer_advance(lexer); // Consume e/E
        
        if (lexer_peek(lexer) == '+' || lexer_peek(lexer) == '-') {
            lexer_advance(lexer);
        }
        
        while (isdigit(lexer_peek(lexer))) {
            lexer_advance(lexer);
        }
    }
    
    Token token = lexer_make_token(lexer, is_float ? TK_FLOAT : TK_INT);
    
    // Parse the value
    char* num_str = str_ncopy(token.start, token.length);
    if (num_str) {
        if (is_float) {
            token.value.float_val = atof(num_str);
        } else {
            token.value.int_val = strtoll(num_str, NULL, 10);
        }
        free(num_str);
    }
    
    return token;
}

bool lexer_is_identifier_char(char c) {
    return isalnum(c) || c == '_' || c == '$' || c == '@';
}

Token lexer_identifier(Lexer* lexer) {
    while (lexer_is_identifier_char(lexer_peek(lexer))) {
        lexer_advance(lexer);
    }
    
    Token token = lexer_make_token(lexer, TK_IDENT);
    
    // Check if it's a keyword
    for (int i = 0; keywords[i].keyword != NULL; i++) {
        if (token.length == keywords[i].length &&
            memcmp(token.start, keywords[i].keyword, token.length) == 0) {
            token.kind = keywords[i].kind;
            break;
        }
    }
    
    return token;
}

Token lexer_next_token(Lexer* lexer) {
    lexer_skip_whitespace(lexer);
    
    lexer->start = lexer->current;
    
    if (lexer_is_at_end(lexer)) {
        return lexer_make_token(lexer, TK_EOF);
    }
    
    char c = lexer_advance(lexer);
    
    if (isdigit(c)) {
        return lexer_number(lexer);
    }
    
    if (isalpha(c) || c == '_' || c == '$' || c == '@') {
        return lexer_identifier(lexer);
    }
    
    switch (c) {
        case '(': return lexer_make_token(lexer, TK_LPAREN);
        case ')': return lexer_make_token(lexer, TK_RPAREN);
        case '{': return lexer_make_token(lexer, TK_LBRACE);
        case '}': return lexer_make_token(lexer, TK_RBRACE);
        case '[': return lexer_make_token(lexer, TK_LSQUARE);
        case ']': return lexer_make_token(lexer, TK_RSQUARE);
        case ',': return lexer_make_token(lexer, TK_COMMA);
        case '.': 
            if (lexer_match(lexer, '.')) {
                if (lexer_match(lexer, '.')) {
                    return lexer_make_token(lexer, TK_ELLIPSIS);
                } else if (lexer_match(lexer, '=')) {
                    return lexer_make_token(lexer, TK_RANGE_INCL);
                } else {
                    return lexer_make_token(lexer, TK_RANGE);
                }
            }
            return lexer_make_token(lexer, TK_PERIOD);
        case ';': return lexer_make_token(lexer, TK_SEMICOLON);
        case ':': 
            if (lexer_match(lexer, ':')) {
                return lexer_make_token(lexer, TK_SCOPE);
            }
            return lexer_make_token(lexer, TK_COLON);
        case '?': 
            if (lexer_match(lexer, '.')) {
                return lexer_make_token(lexer, TK_SAFE_NAV);
            }
            if (lexer_match(lexer, '?')) {
                return lexer_make_token(lexer, TK_NULLISH);
            }
            return lexer_make_token(lexer, TK_QUESTION);
        case '!':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_NEQ);
            }
            return lexer_make_token(lexer, TK_NOT);
        case '=':
            if (lexer_match(lexer, '=')) {
                if (lexer_match(lexer, '=')) {
                    return lexer_make_token(lexer, TK_SPACESHIP);
                }
                return lexer_make_token(lexer, TK_EQ);
            }
            if (lexer_match(lexer, '>')) {
                return lexer_make_token(lexer, TK_RARROW);
            }
            return lexer_make_token(lexer, TK_ASSIGN);
        case '<':
            if (lexer_match(lexer, '=')) {
                if (lexer_match(lexer, '>')) {
                    return lexer_make_token(lexer, TK_SPACESHIP);
                }
                return lexer_make_token(lexer, TK_LTE);
            }
            if (lexer_match(lexer, '<')) {
                if (lexer_match(lexer, '=')) {
                    return lexer_make_token(lexer, TK_SHL);
                }
                return lexer_make_token(lexer, TK_SHL);
            }
            if (lexer_match(lexer, '-')) {
                return lexer_make_token(lexer, TK_LDARROW);
            }
            return lexer_make_token(lexer, TK_LT);
        case '>':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_GTE);
            }
            if (lexer_match(lexer, '>')) {
                if (lexer_match(lexer, '>')) {
                    if (lexer_match(lexer, '=')) {
                        return lexer_make_token(lexer, TK_USHR);
                    }
                    return lexer_make_token(lexer, TK_USHR);
                }
                if (lexer_match(lexer, '=')) {
                    return lexer_make_token(lexer, TK_SHR);
                }
                return lexer_make_token(lexer, TK_SHR);
            }
            return lexer_make_token(lexer, TK_GT);
        case '+':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_PLUS_ASSIGN);
            }
            if (lexer_match(lexer, '+')) {
                return lexer_make_token(lexer, TK_INCREMENT);
            }
            return lexer_make_token(lexer, TK_PLUS);
        case '-':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_MINUS_ASSIGN);
            }
            if (lexer_match(lexer, '-')) {
                return lexer_make_token(lexer, TK_DECREMENT);
            }
            if (lexer_match(lexer, '>')) {
                return lexer_make_token(lexer, TK_DARROW);
            }
            return lexer_make_token(lexer, TK_MINUS);
        case '*':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_MULT_ASSIGN);
            }
            if (lexer_match(lexer, '*')) {
                if (lexer_match(lexer, '=')) {
                    return lexer_make_token(lexer, TK_POW_ASSIGN);
                }
                return lexer_make_token(lexer, TK_POW);
            }
            return lexer_make_token(lexer, TK_MULT);
        case '/':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_DIV_ASSIGN);
            }
            return lexer_make_token(lexer, TK_DIV);
        case '%':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_MOD_ASSIGN);
            }
            return lexer_make_token(lexer, TK_MOD);
        case '&':
            if (lexer_match(lexer, '&')) {
                return lexer_make_token(lexer, TK_AND);
            }
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_BIT_AND);
            }
            return lexer_make_token(lexer, TK_BIT_AND);
        case '|':
            if (lexer_match(lexer, '|')) {
                return lexer_make_token(lexer, TK_OR);
            }
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_BIT_OR);
            }
            return lexer_make_token(lexer, TK_BIT_OR);
        case '^':
            if (lexer_match(lexer, '=')) {
                return lexer_make_token(lexer, TK_BIT_XOR);
            }
            return lexer_make_token(lexer, TK_BIT_XOR);
        case '~':
            return lexer_make_token(lexer, TK_BIT_NOT);
        case '"':
            return lexer_string(lexer);
        case '\'':
            // Character literal
            if (lexer_peek(lexer) == '\\') {
                lexer_advance(lexer); // Skip backslash
                // Handle escape
                switch (lexer_peek(lexer)) {
                    case 'n': c = '\n'; break;
                    case 't': c = '\t'; break;
                    case 'r': c = '\r'; break;
                    case '\\': c = '\\'; break;
                    case '\'': c = '\''; break;
                    default: c = lexer_peek(lexer); break;
                }
                lexer_advance(lexer);
            } else {
                c = lexer_advance(lexer);
            }
            if (lexer_peek(lexer) != '\'') {
                return lexer_error_token(lexer, "Unterminated character literal");
            }
            lexer_advance(lexer); // Skip closing quote
            
            Token token = lexer_make_token(lexer, TK_INT);
            token.value.int_val = c;
            return token;
        case '@': return lexer_make_token(lexer, TK_AT);
        case '$': return lexer_make_token(lexer, TK_DOLLAR);
        case '`': return lexer_make_token(lexer, TK_BACKTICK);
        case '\\':
            // Could be line continuation or lambda
            if (lexer_peek(lexer) == '\n') {
                lexer_advance(lexer); // Skip newline
                lexer->line++;
                lexer->column = 1;
                return lexer_next_token(lexer); // Skip to next token
            }
            break;
    }
    
    return lexer_error_token(lexer, "Unexpected character");
}

Token lexer_peek_token(Lexer* lexer) {
    Lexer saved = *lexer;
    Token token = lexer_next_token(lexer);
    *lexer = saved;
    return token;
}

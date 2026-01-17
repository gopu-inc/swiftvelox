#include "lexer.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <ctype.h>

// Variables globales
Scanner scanner;
Token current_token;
Token previous_token;
int had_error = 0;
int panic_mode = 0;

void init_scanner(const char* source) {
    scanner.start = source;
    scanner.current = source;
    scanner.line = 1;
    scanner.col = 1;
}

char advance() {
    scanner.current++;
    scanner.col++;
    return scanner.current[-1];
}

char peek() { return *scanner.current; }
char peek_next() { 
    return scanner.current[1] ? scanner.current[1] : '\0'; 
}

int is_at_end() { return *scanner.current == '\0'; }

Token make_token(TokenType type) {
    Token token;
    token.type = type;
    token.start = (char*)scanner.start;
    token.length = (int)(scanner.current - scanner.start);
    token.line = scanner.line;
    token.col = scanner.col - token.length;
    return token;
}

Token error_token(const char* message) {
    Token token;
    token.type = TK_ERROR;
    token.s = (char*)message;
    token.line = scanner.line;
    token.col = scanner.col;
    return token;
}

void skip_whitespace() {
    for (;;) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\r':
            case '\t':
                advance();
                scanner.col++;
                break;
            case '\n':
                scanner.line++;
                scanner.col = 1;
                advance();
                break;
            case '/':
                if (peek_next() == '/') {
                    while (peek() != '\n' && !is_at_end()) advance();
                } else if (peek_next() == '*') {
                    advance(); // Skip /
                    advance(); // Skip *
                    while (!(peek() == '*' && peek_next() == '/') && !is_at_end()) {
                        if (peek() == '\n') {
                            scanner.line++;
                            scanner.col = 1;
                        }
                        advance();
                    }
                    if (!is_at_end()) {
                        advance(); // Skip *
                        advance(); // Skip /
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

TokenType check_keyword(int start, int length, const char* rest, TokenType type) {
    if (scanner.current - scanner.start == start + length &&
        memcmp(scanner.start + start, rest, length) == 0) {
        return type;
    }
    return TK_IDENTIFIER;
}

TokenType identifier_type() {
    char* start = (char*)scanner.start;
    int length = scanner.current - scanner.start;
    
    switch (start[0]) {
        case 'a':
            if (length == 5 && memcmp(start, "async", 5) == 0) return TK_ASYNC;
            if (length == 5 && memcmp(start, "await", 5) == 0) return TK_AWAIT;
            if (length == 4 && memcmp(start, "as", 2) == 0) return check_keyword(2, 2, "", TK_AS);
            break;
        case 'b': 
            if (length == 5 && memcmp(start, "break", 5) == 0) return TK_BREAK;
            if (length == 4 && memcmp(start, "bool", 4) == 0) return TK_BOOL_TYPE;
            break;
        case 'c':
            if (length == 4 && memcmp(start, "case", 4) == 0) return TK_CASE;
            if (length == 5 && memcmp(start, "catch", 5) == 0) return TK_CATCH;
            if (length == 5 && memcmp(start, "class", 5) == 0) return TK_CLASS;
            if (length == 5 && memcmp(start, "const", 5) == 0) return TK_CONST;
            if (length == 8 && memcmp(start, "continue", 8) == 0) return TK_CONTINUE;
            if (length == 11 && memcmp(start, "constructor", 11) == 0) return TK_CONSTRUCTOR;
            break;
        case 'd':
            if (length == 6 && memcmp(start, "delete", 6) == 0) return TK_DELETE;
            if (length == 2 && memcmp(start, "do", 2) == 0) return TK_DO;
            if (length == 7 && memcmp(start, "default", 7) == 0) return TK_DEFAULT;
            break;
        case 'e':
            if (length == 4 && memcmp(start, "else", 4) == 0) return TK_ELSE;
            if (length == 4 && memcmp(start, "enum", 4) == 0) return TK_ENUM;
            if (length == 6 && memcmp(start, "export", 6) == 0) return TK_EXPORT;
            if (length == 7 && memcmp(start, "extends", 7) == 0) return TK_EXTENDS;
            break;
        case 'f':
            if (length == 2 && memcmp(start, "fn", 2) == 0) return TK_FN;
            if (length == 5 && memcmp(start, "false", 5) == 0) return TK_FALSE;
            if (length == 3 && memcmp(start, "for", 3) == 0) return TK_FOR;
            if (length == 5 && memcmp(start, "final", 5) == 0) return TK_FINAL;
            if (length == 7 && memcmp(start, "finally", 7) == 0) return TK_FINALLY;
            if (length == 5 && memcmp(start, "float", 5) == 0) return TK_FLOAT_TYPE;
            if (length == 4 && memcmp(start, "from", 4) == 0) return TK_FROM;
            break;
        case 'i':
            if (length == 2 && memcmp(start, "if", 2) == 0) return TK_IF;
            if (length == 2 && memcmp(start, "in", 2) == 0) return TK_IN;
            if (length == 6 && memcmp(start, "import", 6) == 0) return TK_IMPORT;
            if (length == 3 && memcmp(start, "int", 3) == 0) return TK_INT_TYPE;
            if (length == 9 && memcmp(start, "interface", 9) == 0) return TK_INTERFACE;
            if (length == 10 && memcmp(start, "implements", 10) == 0) return TK_IMPLEMENTS;
            if (length == 10 && memcmp(start, "instanceof", 10) == 0) return TK_INSTANCEOF;
            break;
        case 'l':
            if (length == 3 && memcmp(start, "let", 3) == 0) return TK_LET;
            if (length == 3 && memcmp(start, "log", 3) == 0) return TK_LOG;
            break;
        case 'm':
            if (length == 5 && memcmp(start, "match", 5) == 0) return TK_MATCH;
            if (length == 5 && memcmp(start, "macro", 5) == 0) return TK_MACRO;
            if (length == 6 && memcmp(start, "module", 6) == 0) return TK_MODULE;
            if (length == 3 && memcmp(start, "mut", 3) == 0) return TK_MUT;
            break;
        case 'n':
            if (length == 3 && memcmp(start, "new", 3) == 0) return TK_NEW;
            if (length == 3 && memcmp(start, "nil", 3) == 0) return TK_NIL;
            if (length == 9 && memcmp(start, "namespace", 9) == 0) return TK_NAMESPACE;
            break;
        case 'o':
            if (length == 2 && memcmp(start, "or", 2) == 0) return TK_OR;
            break;
        case 'p':
            if (length == 6 && memcmp(start, "public", 6) == 0) return TK_PUBLIC;
            if (length == 7 && memcmp(start, "private", 7) == 0) return TK_PRIVATE;
            if (length == 9 && memcmp(start, "protected", 9) == 0) return TK_PROTECTED;
            break;
        case 'r':
            if (length == 6 && memcmp(start, "return", 6) == 0) return TK_RETURN;
            if (length == 6 && memcmp(start, "repeat", 6) == 0) return TK_REPEAT;
            break;
        case 's':
            if (length == 6 && memcmp(start, "static", 6) == 0) return TK_STATIC;
            if (length == 6 && memcmp(start, "string", 6) == 0) return TK_STRING_TYPE;
            if (length == 6 && memcmp(start, "struct", 6) == 0) return TK_STRUCT;
            if (length == 5 && memcmp(start, "super", 5) == 0) return TK_SUPER;
            break;
        case 't':
            if (length == 4 && memcmp(start, "true", 4) == 0) return TK_TRUE;
            if (length == 4 && memcmp(start, "this", 4) == 0) return TK_THIS;
            if (length == 5 && memcmp(start, "throw", 5) == 0) return TK_THROW;
            if (length == 3 && memcmp(start, "try", 3) == 0) return TK_TRY;
            if (length == 6 && memcmp(start, "typeof", 6) == 0) return TK_TYPEOF;
            if (length == 5 && memcmp(start, "trait", 5) == 0) return TK_TRAIT;
            break;
        case 'u':
            if (length == 6 && memcmp(start, "unless", 6) == 0) return TK_UNLESS;
            if (length == 5 && memcmp(start, "until", 5) == 0) return TK_UNTIL;
            if (length == 9 && memcmp(start, "undefined", 9) == 0) return TK_UNDEFINED;
            break;
        case 'v':
            if (length == 3 && memcmp(start, "var", 3) == 0) return TK_VAR;
            if (length == 4 && memcmp(start, "void", 4) == 0) return TK_VOID;
            break;
        case 'w':
            if (length == 5 && memcmp(start, "while", 5) == 0) return TK_WHILE;
            if (length == 4 && memcmp(start, "when", 4) == 0) return TK_WHEN;
            if (length == 5 && memcmp(start, "where", 5) == 0) return TK_WHERE;
            if (length == 4 && memcmp(start, "with", 4) == 0) return TK_WITH;
            break;
        case 'y':
            if (length == 5 && memcmp(start, "yield", 5) == 0) return TK_YIELD;
            break;
    }
    
    return TK_IDENTIFIER;
}

Token scan_token() {
    skip_whitespace();
    scanner.start = scanner.current;
    
    if (is_at_end()) return make_token(TK_EOF);
    
    char c = advance();
    
    // Identifiers and keywords
    if (isalpha(c) || c == '_') {
        while (isalnum(peek()) || peek() == '_') advance();
        Token t = make_token(identifier_type());
        if (t.type == TK_IDENTIFIER) {
            t.s = malloc(t.length + 1);
            memcpy(t.s, t.start, t.length);
            t.s[t.length] = 0;
        }
        return t;
    }
    
    // Numbers
    if (isdigit(c)) {
        while (isdigit(peek())) advance();
        
        if (peek() == '.' && isdigit(peek_next())) {
            advance(); // Consume '.'
            while (isdigit(peek())) advance();
            
            // Scientific notation
            if (peek() == 'e' || peek() == 'E') {
                advance();
                if (peek() == '+' || peek() == '-') advance();
                while (isdigit(peek())) advance();
            }
            
            Token t = make_token(TK_NUMBER);
            char* num_str = malloc(t.length + 1);
            memcpy(num_str, t.start, t.length);
            num_str[t.length] = 0;
            t.d = strtod(num_str, NULL);
            free(num_str);
            return t;
        }
        
        Token t = make_token(TK_NUMBER);
        char* num_str = malloc(t.length + 1);
        memcpy(num_str, t.start, t.length);
        num_str[t.length] = 0;
        t.i = strtoll(num_str, NULL, 10);
        free(num_str);
        return t;
    }
    
    // Strings
    if (c == '"' || c == '\'') {
        char quote = c;
        while (peek() != quote && !is_at_end()) {
            if (peek() == '\\') advance();
            advance();
        }
        
        if (is_at_end()) return error_token("Chaîne non terminée");
        
        advance(); // Consume closing quote
        
        Token t = make_token(TK_STRING_LIT);
        t.s = malloc(t.length - 1); // Exclude quotes
        memcpy(t.s, t.start + 1, t.length - 2);
        t.s[t.length - 2] = 0;
        return t;
    }
    
    // Template strings
    if (c == '`') {
        while (peek() != '`' && !is_at_end()) {
            if (peek() == '\\') advance();
            if (peek() == '$' && peek_next() == '{') {
                // Template interpolation
                advance(); // Skip $
                advance(); // Skip {
                // TODO: Parse expression
                while (peek() != '}' && !is_at_end()) advance();
                if (peek() == '}') advance();
            }
            advance();
        }
        
        if (is_at_end()) return error_token("Template non terminé");
        advance(); // Consume closing `
        
        Token t = make_token(TK_TEMPLATE_LIT);
        t.s = malloc(t.length - 1);
        memcpy(t.s, t.start + 1, t.length - 2);
        t.s[t.length - 2] = 0;
        return t;
    }
    
    // Operators and punctuation
    switch (c) {
        case '(': return make_token(TK_LPAREN);
        case ')': return make_token(TK_RPAREN);
        case '{': return make_token(TK_LBRACE);
        case '}': return make_token(TK_RBRACE);
        case '[': return make_token(TK_LBRACKET);
        case ']': return make_token(TK_RBRACKET);
        case ';': return make_token(TK_SEMICOLON);
        case ',': return make_token(TK_COMMA);
        case '.': 
            if (peek() == '.') {
                advance();
                if (peek() == '.') {
                    advance();
                    return make_token(TK_DOTDOTDOT);
                }
                return make_token(TK_DOTDOT);
            }
            return make_token(TK_DOT);
        case '-':
            if (peek() == '>') {
                advance();
                if (peek() == '>') {
                    advance();
                    return make_token(TK_DOUBLE_ARROW);
                }
                return make_token(TK_ARROW);
            }
            if (peek() == '=') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_MINUS_EQ_EQ);
                }
                return make_token(TK_MINUS_EQ);
            }
            if (peek() == '-') {
                advance();
                return make_token(TK_MINUS_MINUS);
            }
            return make_token(TK_MINUS);
        case '+':
            if (peek() == '>') {
                advance();
                return make_token(TK_PIPELINE);
            }
            if (peek() == '=') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_PLUS_EQ_EQ);
                }
                return make_token(TK_PLUS_EQ);
            }
            if (peek() == '+') {
                advance();
                return make_token(TK_PLUS_PLUS);
            }
            return make_token(TK_PLUS);
        case '*':
            if (peek() == '=') {
                advance();
                return make_token(TK_STAR_EQ);
            }
            if (peek() == '*') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_STAR_EQ); // **=
                }
                return make_token(TK_STAR); // **
            }
            return make_token(TK_STAR);
        case '/':
            if (peek() == '=') {
                advance();
                return make_token(TK_SLASH_EQ);
            }
            return make_token(TK_SLASH);
        case '%':
            if (peek() == '=') {
                advance();
                return make_token(TK_PERCENT);
            }
            return make_token(TK_PERCENT);
        case '=':
            if (peek() == '=') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_EQEQEQ);
                }
                return make_token(TK_EQEQ);
            }
            if (peek() == '>') {
                advance();
                return make_token(TK_FAT_ARROW);
            }
            return make_token(TK_EQ);
        case '!':
            if (peek() == '=') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_BANGEQEQ);
                }
                return make_token(TK_BANGEQ);
            }
            return make_token(TK_BANG);
        case '<':
            if (peek() == '=') {
                advance();
                return make_token(TK_LTEQ);
            }
            if (peek() == '<') {
                advance();
                if (peek() == '=') {
                    advance();
                    return make_token(TK_LSHIFT_EQ);
                }
                return make_token(TK_LSHIFT);
            }
            return make_token(TK_LT);
        case '>':
            if (peek() == '=') {
                advance();
                return make_token(TK_GTEQ);
            }
            if (peek() == '>') {
                advance();
                if (peek() == '>') {
                    advance();
                    if (peek() == '=') {
                        advance();
                        return make_token(TK_URSHIFT_EQ);
                    }
                    return make_token(TK_URSHIFT);
                }
                if (peek() == '=') {
                    advance();
                    return make_token(TK_RSHIFT_EQ);
                }
                return make_token(TK_RSHIFT);
            }
            return make_token(TK_GT);
        case '&':
            if (peek() == '&') {
                advance();
                return make_token(TK_AND);
            }
            if (peek() == '=') {
                advance();
                return make_token(TK_AMPERSAND_EQ);
            }
            return make_token(TK_AMPERSAND);
        case '|':
            if (peek() == '|') {
                advance();
                return make_token(TK_OR);
            }
            if (peek() == '=') {
                advance();
                return make_token(TK_BAR_EQ);
            }
            if (peek() == '>') {
                advance();
                return make_token(TK_PIPE);
            }
            return make_token(TK_BAR);
        case '^':
            if (peek() == '=') {
                advance();
                return make_token(TK_CARET_EQ);
            }
            return make_token(TK_CARET);
        case '~': return make_token(TK_TILDE);
        case '?':
            if (peek() == '?') {
                advance();
                return make_token(TK_DOUBLE_QUESTION);
            }
            if (peek() == '.') {
                advance();
                return make_token(TK_QUESTION); // ?.
            }
            return make_token(TK_QUESTION);
        case ':':
            if (peek() == ':') {
                advance();
                return make_token(TK_DOUBLE_COLON);
            }
            return make_token(TK_COLON);
        case '@': return make_token(TK_AT);
        case '#': return make_token(TK_HASH);
        case '$': return make_token(TK_DOLLAR);
        case '_': return make_token(TK_UNDERSCORE);
    }
    
    return error_token("Caractère inattendu");
}

Token next_token() { 
    previous_token = current_token; 
    current_token = scan_token(); 
    return current_token; 
}

int match(TokenType type) { 
    if (current_token.type == type) { 
        next_token(); 
        return 1; 
    } 
    return 0; 
}

void consume(TokenType type, const char* message) { 
    if (current_token.type == type) { 
        next_token(); 
        return; 
    } 
    syntax_error(current_token, message);
}

void syntax_error(Token token, const char* message) {
    if (panic_mode) return;
    panic_mode = 1;
    
    fprintf(stderr, "\033[1;31m[Ligne %d] Erreur de syntaxe", token.line);
    if (token.type == TK_EOF) {
        fprintf(stderr, " à la fin");
    } else if (token.type == TK_ERROR) {
        fprintf(stderr, ": %s", token.s);
    } else {
        fprintf(stderr, " à '%.*s'", token.length, token.start);
    }
    fprintf(stderr, ": %s\033[0m\n", message);
    had_error = 1;
}

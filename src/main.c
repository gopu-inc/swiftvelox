#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_TOKENS 10000

// Types
typedef enum {
    TK_FN, TK_LET, TK_IF, TK_ELSE, TK_RETURN,
    TK_IMPORT, TK_FOR, TK_WHILE, TK_TRUE, TK_FALSE,
    TK_I32, TK_I64, TK_STRING, TK_BOOL, TK_VOID,
    TK_IDENT, TK_INTLIT, TK_STRLIT,
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_AND, TK_OR, TK_NOT, TK_ASSIGN,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_SEMICOLON, TK_COLON, TK_COMMA, TK_EOF, TK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[256];
    int line;
    int col;
    long int_val;
} Token;

// Runtime simple
void sv_print(const char* msg) {
    printf("%s\n", msg);
}

char* int_to_str(long n) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%ld", n);
    return buf;
}

char* str_concat(const char* a, const char* b) {
    char* r = malloc(strlen(a) + strlen(b) + 1);
    strcpy(r, a);
    strcat(r, b);
    return r;
}

// Lexer
Token* lexer(const char* source) {
    static Token tokens[MAX_TOKENS];
    int count = 0, i = 0, line = 1, col = 1;
    int len = source ? strlen(source) : 0;
    
    if (!source) {
        tokens[0].type = TK_EOF;
        return tokens;
    }
    
    while (i < len && count < MAX_TOKENS-1) {
        char ch = source[i];
        
        if (isspace(ch)) {
            if (ch == '\n') { line++; col = 1; }
            i++; col++;
            continue;
        }
        
        if (ch == '/' && i+1 < len && source[i+1] == '/') {
            i += 2;
            while (i < len && source[i] != '\n') i++;
            col = 1;
            continue;
        }
        
        Token* tok = &tokens[count];
        tok->line = line;
        tok->col = col;
        tok->lexeme[0] = '\0';
        
        // Identifiants
        if (isalpha(ch) || ch == '_') {
            int j = 0;
            while (i < len && (isalnum(source[i]) || source[i] == '_')) {
                if (j < 255) tok->lexeme[j++] = source[i];
                i++; col++;
            }
            tok->lexeme[j] = '\0';
            
            if (strcmp(tok->lexeme, "fn") == 0) tok->type = TK_FN;
            else if (strcmp(tok->lexeme, "let") == 0) tok->type = TK_LET;
            else if (strcmp(tok->lexeme, "if") == 0) tok->type = TK_IF;
            else if (strcmp(tok->lexeme, "else") == 0) tok->type = TK_ELSE;
            else if (strcmp(tok->lexeme, "return") == 0) tok->type = TK_RETURN;
            else if (strcmp(tok->lexeme, "sw") == 0) tok->type = TK_IMPORT;
            else if (strcmp(tok->lexeme, "as") == 0) tok->type = TK_IDENT;
            else if (strcmp(tok->lexeme, "string") == 0) tok->type = TK_STRING;
            else tok->type = TK_IDENT;
            
            count++;
            continue;
        }
        
        // Nombres
        if (isdigit(ch)) {
            int j = 0;
            while (i < len && isdigit(source[i])) {
                if (j < 255) tok->lexeme[j++] = source[i];
                i++; col++;
            }
            tok->lexeme[j] = '\0';
            tok->type = TK_INTLIT;
            tok->int_val = atol(tok->lexeme);
            count++;
            continue;
        }
        
        // Chaînes
        if (ch == '"') {
            int j = 0;
            i++; col++;
            while (i < len && source[i] != '"') {
                if (j < 255) tok->lexeme[j++] = source[i];
                i++; col++;
            }
            if (i < len && source[i] == '"') { i++; col++; }
            tok->lexeme[j] = '\0';
            tok->type = TK_STRLIT;
            count++;
            continue;
        }
        
        // Opérateurs
        switch (ch) {
            case '+': tok->type = TK_PLUS; strcpy(tok->lexeme, "+"); break;
            case '-': tok->type = TK_MINUS; strcpy(tok->lexeme, "-"); break;
            case '*': tok->type = TK_MUL; strcpy(tok->lexeme, "*"); break;
            case '/': tok->type = TK_DIV; strcpy(tok->lexeme, "/"); break;
            case '(': tok->type = TK_LPAREN; strcpy(tok->lexeme, "("); break;
            case ')': tok->type = TK_RPAREN; strcpy(tok->lexeme, ")"); break;
            case '{': tok->type = TK_LBRACE; strcpy(tok->lexeme, "{"); break;
            case '}': tok->type = TK_RBRACE; strcpy(tok->lexeme, "}"); break;
            case ';': tok->type = TK_SEMICOLON; strcpy(tok->lexeme, ";"); break;
            case ':': tok->type = TK_COLON; strcpy(tok->lexeme, ":"); break;
            case ',': tok->type = TK_COMMA; strcpy(tok->lexeme, ","); break;
            case '=': 
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_EQ;
                    strcpy(tok->lexeme, "==");
                    i++; col++;
                } else {
                    tok->type = TK_ASSIGN;
                    strcpy(tok->lexeme, "=");
                }
                break;
            case '!':
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_NEQ;
                    strcpy(tok->lexeme, "!=");
                    i++; col++;
                } else {
                    tok->type = TK_NOT;
                    strcpy(tok->lexeme, "!");
                }
                break;
            case '<':
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_LE;
                    strcpy(tok->lexeme, "<=");
                    i++; col++;
                } else {
                    tok->type = TK_LT;
                    strcpy(tok->lexeme, "<");
                }
                break;
            case '>':
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_GE;
                    strcpy(tok->lexeme, ">=");
                    i++; col++;
                } else {
                    tok->type = TK_GT;
                    strcpy(tok->lexeme, ">");
                }
                break;
            default: 
                tok->type = TK_ERROR;
                tok->lexeme[0] = ch;
                tok->lexeme[1] = '\0';
                break;
        }
        
        i++; col++;
        count++;
    }
    
    tokens[count].type = TK_EOF;
    tokens[count].lexeme[0] = '\0';
    
    return tokens;
}

// Générateur de code
void generate_simple_c(Token* tokens, const char* output_file) {
    FILE* f = fopen(output_file, "w");
    if (!f) {
        printf("Cannot create %s\n", output_file);
        return;
    }
    
    fprintf(f, "// Generated by SwiftVelox\n");
    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n");
    fprintf(f, "#include <string.h>\n\n");
    
    fprintf(f, "void sv_print(const char* s) { printf(\"%%s\\n\", s); }\n\n");
    fprintf(f, "char* int_str(long n) {\n");
    fprintf(f, "    static char buf[32];\n");
    fprintf(f, "    sprintf(buf, \"%%ld\", n);\n");
    fprintf(f, "    return buf;\n");
    fprintf(f, "}\n\n");
    fprintf(f, "char* cat_str(const char* a, const char* b) {\n");
    fprintf(f, "    char* r = malloc(strlen(a)+strlen(b)+1);\n");
    fprintf(f, "    strcpy(r, a);\n");
    fprintf(f, "    strcat(r, b);\n");
    fprintf(f, "    return r;\n");
    fprintf(f, "}\n\n");
    
    fprintf(f, "int main() {\n");
    
    int i = 0;
    int in_swget = 0;
    char print_buffer[1024] = "";
    
    while (tokens[i].type != TK_EOF) {
        Token tok = tokens[i];
        
        if (tok.type == TK_IDENT && strcmp(tok.lexeme, "swget") == 0) {
            in_swget = 1;
            i += 2; // skip '('
            print_buffer[0] = '\0';
        }
        else if (in_swget && tok.type == TK_STRLIT) {
            snprintf(print_buffer, sizeof(print_buffer), "\"%s\"", tok.lexeme);
        }
        else if (in_swget && tok.type == TK_IDENT) {
            // Vérifier si c'est "var as string"
            if (i+2 < MAX_TOKENS && 
                tokens[i+1].type == TK_IDENT && 
                strcmp(tokens[i+1].lexeme, "as") == 0 &&
                tokens[i+2].type == TK_IDENT && 
                strcmp(tokens[i+2].lexeme, "string") == 0) {
                
                char* var = tok.lexeme;
                i += 3; // skip var, as, string
                
                if (strlen(print_buffer) > 0) {
                    fprintf(f, "    sv_print(cat_str(%s, int_str(%s)));\n", print_buffer, var);
                } else {
                    fprintf(f, "    sv_print(int_str(%s));\n", var);
                }
                in_swget = 0;
            } else {
                snprintf(print_buffer, sizeof(print_buffer), "\"%s\"", tok.lexeme);
            }
        }
        else if (in_swget && tok.type == TK_PLUS) {
            i++;
        }
        else if (in_swget && tok.type == TK_RPAREN) {
            if (print_buffer[0] == '"') {
                fprintf(f, "    sv_print(%s);\n", print_buffer);
            }
            in_swget = 0;
        }
        else if (tok.type == TK_LET) {
            i++;
            if (tokens[i].type == TK_IDENT) {
                char* var = tokens[i].lexeme;
                i += 2;
                
                if (tokens[i].type == TK_INTLIT) {
                    fprintf(f, "    long %s = %ld;\n", var, tokens[i].int_val);
                }
                else if (tokens[i].type == TK_STRLIT) {
                    fprintf(f, "    const char* %s = \"%s\";\n", var, tokens[i].lexeme);
                }
            }
        }
        else if (tok.type == TK_IF) {
            i++;
            fprintf(f, "    if (");
            
            if (tokens[i].type == TK_IDENT) {
                char* left = tokens[i].lexeme;
                i++;
                char* op = "";
                if (tokens[i].type == TK_GT) op = ">";
                else if (tokens[i].type == TK_LT) op = "<";
                i++;
                
                if (tokens[i].type == TK_IDENT) {
                    fprintf(f, "%s %s %s", left, op, tokens[i].lexeme);
                } else if (tokens[i].type == TK_INTLIT) {
                    fprintf(f, "%s %s %ld", left, op, tokens[i].int_val);
                }
            }
            
            fprintf(f, ") {\n");
        }
        else if (tok.type == TK_LBRACE) {
            fprintf(f, "    {\n");
        }
        else if (tok.type == TK_RBRACE) {
            fprintf(f, "    }\n");
        }
        else if (tok.type == TK_ELSE) {
            fprintf(f, "    } else {\n");
        }
        
        i++;
    }
    
    fprintf(f, "    return 0;\n");
    fprintf(f, "}\n");
    fclose(f);
}

int compile_file(const char* input) {
    printf("Compiling: %s\n", input);
    
    FILE* f = fopen(input, "r");
    if (!f) {
        printf("File not found\n");
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    Token* tokens = lexer(source);
    
    char c_file[256];
    snprintf(c_file, sizeof(c_file), "%s.c", input);
    
    generate_simple_c(tokens, c_file);
    free(source);
    
    printf("C code: %s\n", c_file);
    return 0;
}

void compile_and_run(const char* filename) {
    if (compile_file(filename) != 0) return;
    
    char c_file[256], exe_file[256];
    snprintf(c_file, sizeof(c_file), "%s.c", filename);
    snprintf(exe_file, sizeof(exe_file), "%s.out", filename);
    
    printf("Building C...\n");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O2 -o %s %s 2>&1", exe_file, c_file);
    
    int result = system(cmd);
    if (result != 0) {
        snprintf(cmd, sizeof(cmd), "cc -o %s %s", exe_file, c_file);
        system(cmd);
    }
    
    // Vérifier si le fichier existe
    FILE* test = fopen(exe_file, "r");
    if (test) {
        fclose(test);
        printf("Executable: %s\n", exe_file);
        printf("Running...\n\n");
        snprintf(cmd, sizeof(cmd), "./%s", exe_file);
        system(cmd);
    } else {
        printf("Build failed\n");
    }
}

int main(int argc, char** argv) {
    if (argc < 2) {
        printf("SwiftVelox Compiler\n");
        printf("Usage: %s <file.svx>\n", argv[0]);
        printf("       %s run <file.svx>\n", argv[0]);
        return 1;
    }
    
    if (strcmp(argv[1], "run") == 0) {
        if (argc < 3) {
            printf("File required\n");
            return 1;
        }
        compile_and_run(argv[2]);
    } else {
        compile_and_run(argv[1]);
    }
    
    return 0;
}

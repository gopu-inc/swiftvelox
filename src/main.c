#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>

#define MAX_TOKENS 10000

// Types de tokens
typedef enum {
    TK_FN, TK_LET, TK_IF, TK_ELSE, TK_RETURN,
    TK_IMPORT, TK_FOR, TK_WHILE, TK_TRUE, TK_FALSE,
    TK_I32, TK_I64, TK_F32, TK_F64, TK_STRING, TK_BOOL, TK_VOID,
    TK_IDENT, TK_INTLIT, TK_FLOATLIT, TK_STRLIT,
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV, TK_MOD,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_AND, TK_OR, TK_NOT, TK_ASSIGN, TK_ARROW,
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET, TK_COMMA, TK_COLON,
    TK_SEMICOLON, TK_DOT, TK_EOF, TK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[256];
    int line;
    int col;
    int64_t int_val;
    double float_val;
} Token;

// Runtime
void sv_print(const char* msg) {
    printf("%s\n", msg);
}

char* int_to_str(int64_t n) {
    static char buf[32];
    snprintf(buf, sizeof(buf), "%ld", n);
    return buf;
}

char* str_concat(const char* a, const char* b) {
    char* result = malloc(strlen(a) + strlen(b) + 1);
    strcpy(result, a);
    strcat(result, b);
    return result;
}

// Lexer
Token* lexer(const char* source) {
    static Token tokens[MAX_TOKENS];
    int count = 0, i = 0, line = 1, col = 1;
    int len = strlen(source);
    
    while (i < len && count < MAX_TOKENS-1) {
        char ch = source[i];
        
        if (isspace(ch)) {
            if (ch == '\n') { line++; col = 1; }
            else { col++; }
            i++;
            continue;
        }
        
        if (ch == '/' && i+1 < len && source[i+1] == '/') {
            while (i < len && source[i] != '\n') { i++; col++; }
            continue;
        }
        
        Token* tok = &tokens[count];
        tok->line = line;
        tok->col = col;
        
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
            tok->int_val = atoi(tok->lexeme);
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
            case '+': tok->type = TK_PLUS; break;
            case '-': 
                if (i+1 < len && source[i+1] == '>') {
                    tok->type = TK_ARROW;
                    i++; col++;
                    strcpy(tok->lexeme, "->");
                } else {
                    tok->type = TK_MINUS;
                }
                break;
            case '*': tok->type = TK_MUL; break;
            case '/': tok->type = TK_DIV; break;
            case '(': tok->type = TK_LPAREN; break;
            case ')': tok->type = TK_RPAREN; break;
            case '{': tok->type = TK_LBRACE; break;
            case '}': tok->type = TK_RBRACE; break;
            case ';': tok->type = TK_SEMICOLON; break;
            case ':': tok->type = TK_COLON; break;
            case ',': tok->type = TK_COMMA; break;
            case '=': 
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_EQ;
                    i++; col++;
                    strcpy(tok->lexeme, "==");
                } else {
                    tok->type = TK_ASSIGN;
                }
                break;
            case '!':
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_NEQ;
                    i++; col++;
                    strcpy(tok->lexeme, "!=");
                } else {
                    tok->type = TK_NOT;
                }
                break;
            case '<':
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_LE;
                    i++; col++;
                    strcpy(tok->lexeme, "<=");
                } else {
                    tok->type = TK_LT;
                }
                break;
            case '>':
                if (i+1 < len && source[i+1] == '=') {
                    tok->type = TK_GE;
                    i++; col++;
                    strcpy(tok->lexeme, ">=");
                } else {
                    tok->type = TK_GT;
                }
                break;
            case '.': tok->type = TK_DOT; break;
            default: 
                tok->type = TK_ERROR;
                tok->lexeme[0] = ch;
                tok->lexeme[1] = '\0';
                break;
        }
        
        if (strlen(tok->lexeme) == 0) {
            tok->lexeme[0] = ch;
            tok->lexeme[1] = '\0';
        }
        
        i++; col++;
        count++;
    }
    
    tokens[count].type = TK_EOF;
    tokens[count].lexeme[0] = '\0';
    
    return tokens;
}

// Génération de code
void generate_c_code(Token* tokens, const char* output) {
    FILE* f = fopen(output, "w");
    if (!f) {
        printf("❌ Impossible de créer %s\n", output);
        return;
    }
    
    fprintf(f, "// Généré par SwiftVelox\n");
    fprintf(f, "#include <stdio.h>\n");
    fprintf(f, "#include <stdlib.h>\n");
    fprintf(f, "#include <string.h>\n\n");
    
    fprintf(f, "void sv_print(const char* msg) { printf(\"%%s\\n\", msg); }\n");
    fprintf(f, "char* int_to_str(long n) {\n");
    fprintf(f, "    static char buf[32];\n");
    fprintf(f, "    snprintf(buf, sizeof(buf), \"%%ld\", n);\n");
    fprintf(f, "    return buf;\n");
    fprintf(f, "}\n");
    fprintf(f, "char* str_concat(const char* a, const char* b) {\n");
    fprintf(f, "    char* r = malloc(strlen(a)+strlen(b)+1);\n");
    fprintf(f, "    strcpy(r, a); strcat(r, b);\n");
    fprintf(f, "    return r;\n");
    fprintf(f, "}\n\n");
    
    fprintf(f, "int main() {\n");
    
    int i = 0;
    char current_print[1024] = "";
    int in_print = 0;
    
    while (tokens[i].type != TK_EOF) {
        Token tok = tokens[i];
        
        if (tok.type == TK_IDENT && strcmp(tok.lexeme, "swget") == 0) {
            in_print = 1;
            i += 2; // skip '('
            current_print[0] = '\0';
        }
        else if (in_print && tok.type == TK_STRLIT) {
            // Chaîne littérale
            snprintf(current_print, sizeof(current_print), "\"%s\"", tok.lexeme);
        }
        else if (in_print && tok.type == TK_IDENT) {
            // Variable seule
            snprintf(current_print, sizeof(current_print), "\"%s\"", tok.lexeme);
        }
        else if (in_print && tok.type == TK_PLUS) {
            i++; // skip '+'
            
            // Vérifier si c'est 'var as string'
            if (i+2 < MAX_TOKENS && 
                tokens[i].type == TK_IDENT &&
                tokens[i+1].type == TK_IDENT && 
                strcmp(tokens[i+1].lexeme, "as") == 0 &&
                tokens[i+2].type == TK_IDENT &&
                strcmp(tokens[i+2].lexeme, "string") == 0) {
                
                char* var_name = tokens[i].lexeme;
                i += 3; // skip var, 'as', 'string'
                
                if (current_print[0] == '\"' && current_print[strlen(current_print)-1] == '\"') {
                    // Chaîne + variable
                    char temp[1024];
                    snprintf(temp, sizeof(temp), 
                            "str_concat(%s, int_to_str(%s))", 
                            current_print, var_name);
                    strcpy(current_print, temp);
                } else {
                    // Juste la variable
                    snprintf(current_print, sizeof(current_print), 
                            "int_to_str(%s)", var_name);
                }
            }
        }
        else if (in_print && tok.type == TK_RPAREN) {
            if (strlen(current_print) > 0) {
                fprintf(f, "    sv_print(%s);\n", current_print);
            }
            in_print = 0;
            current_print[0] = '\0';
        }
        else if (tok.type == TK_LET) {
            i++; // skip 'let'
            if (tokens[i].type == TK_IDENT) {
                char* var_name = tokens[i].lexeme;
                i += 2; // skip var and '='
                
                if (tokens[i].type == TK_INTLIT) {
                    fprintf(f, "    long %s = %ld;\n", var_name, tokens[i].int_val);
                }
                else if (tokens[i].type == TK_STRLIT) {
                    fprintf(f, "    const char* %s = \"%s\";\n", var_name, tokens[i].lexeme);
                }
            }
        }
        else if (tok.type == TK_IF) {
            i++; // skip 'if'
            fprintf(f, "    if (");
            
            // Condition simple: var op var/number
            if (tokens[i].type == TK_IDENT) {
                char left[256];
                strcpy(left, tokens[i].lexeme);
                i++;
                
                char op[4] = "";
                if (tokens[i].type == TK_GT) strcpy(op, ">");
                else if (tokens[i].type == TK_LT) strcpy(op, "<");
                else if (tokens[i].type == TK_EQ) strcpy(op, "==");
                i++;
                
                if (tokens[i].type == TK_IDENT) {
                    fprintf(f, "%s %s %s", left, op, tokens[i].lexeme);
                }
                else if (tokens[i].type == TK_INTLIT) {
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

int compile_file(const char* input, const char* output) {
    printf("🔨 Compilation: %s\n", input);
    
    FILE* f = fopen(input, "r");
    if (!f) {
        printf("❌ Fichier introuvable\n");
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
    
    char out_c[256];
    if (!output) {
        snprintf(out_c, sizeof(out_c), "%s.c", input);
        output = out_c;
    }
    
    generate_c_code(tokens, output);
    free(source);
    
    printf("✅ Code C généré: %s\n", output);
    return 0;
}

void compile_and_run(const char* filename) {
    char c_file[256], exe_file[256];
    
    snprintf(c_file, sizeof(c_file), "%s.c", filename);
    snprintf(exe_file, sizeof(exe_file), "%s.out", filename);
    
    // Compiler
    if (compile_file(filename, c_file) != 0) return;
    
    // Compiler le C
    printf("📦 Compilation C...\n");
    char cmd[512];
    snprintf(cmd, sizeof(cmd), "gcc -O2 -o %s %s", exe_file, c_file);
    
    if (system(cmd) != 0) {
        printf("⚠️  Utilisation de cc...\n");
        snprintf(cmd, sizeof(cmd), "cc -o %s %s", exe_file, c_file);
        system(cmd);
    }
    
    if (access(exe_file, F_OK) == 0) {
        printf("✅ Exécutable: %s\n", exe_file);
        
        // Exécuter
        printf("🚀 Exécution...\n\n");
        snprintf(cmd, sizeof(cmd), "./%s", exe_file);
        system(cmd);
    } else {
        printf("❌ Échec de la création de l'exécutable\n");
    }
}

void print_help() {
    printf("SwiftVelox Compiler v2.1\n");
    printf("Compile directement vers code natif\n");
    printf("\nUsage: swiftvelox <commande> [fichier]\n");
    printf("Commandes:\n");
    printf("  build <fichier.svx>   Compile en C\n");
    printf("  run <fichier.svx>     Compile et exécute\n");
    printf("  version               Affiche la version\n");
    printf("\nExemples:\n");
    printf("  swiftvelox run examples/test.svx\n");
    printf("  swiftvelox build mon_programme.svx\n");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    char* cmd = argv[1];
    
    if (strcmp(cmd, "version") == 0) {
        printf("SwiftVelox v2.1 - Compilateur natif\n");
    }
    else if (strcmp(cmd, "build") == 0) {
        if (argc < 3) {
            printf("❌ Fichier .svx requis\n");
            return 1;
        }
        compile_file(argv[2], NULL);
    }
    else if (strcmp(cmd, "run") == 0) {
        if (argc < 3) {
            printf("❌ Fichier .svx requis\n");
            return 1;
        }
        compile_and_run(argv[2]);
    }
    else {
        // Si pas de commande, essayer de compiler le fichier
        compile_and_run(argv[1]);
    }
    
    return 0;
}

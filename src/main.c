#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>

#define MAX_TOKENS 10000
#define CODE_BUFFER_SIZE 4096

// ===== STRUCTURES =====
typedef enum {
    // Mots-clés
    TK_FN, TK_LET, TK_IF, TK_ELSE, TK_RETURN,
    TK_IMPORT, TK_FOR, TK_WHILE, TK_TRUE, TK_FALSE,
    
    // Types
    TK_I32, TK_I64, TK_F32, TK_F64, TK_STRING, TK_BOOL, TK_VOID,
    
    // Identifiants et littéraux
    TK_IDENT, TK_INTLIT, TK_FLOATLIT, TK_STRLIT,
    
    // Opérateurs
    TK_PLUS, TK_MINUS, TK_MUL, TK_DIV, TK_MOD,
    TK_EQ, TK_NEQ, TK_LT, TK_GT, TK_LE, TK_GE,
    TK_AND, TK_OR, TK_NOT,
    TK_ASSIGN, TK_ARROW,
    
    // Ponctuation
    TK_LPAREN, TK_RPAREN, TK_LBRACE, TK_RBRACE,
    TK_LBRACKET, TK_RBRACKET, TK_COMMA, TK_COLON,
    TK_SEMICOLON, TK_DOT,
    
    // Spécial
    TK_EOF, TK_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char lexeme[256];
    int line;
    int col;
    int64_t int_val;
    double float_val;
} Token;

typedef struct {
    char* name;
    int offset;
    int size;
} Symbol;

// ===== VARIABLES GLOBALES =====
static uint8_t code_buffer[CODE_BUFFER_SIZE];
static int code_pos = 0;
static Symbol symbols[100];
static int symbol_count = 0;
static char* strings[100];
static int string_count = 0;

// ===== FONCTIONS D'ÉMISSION DE CODE =====
void emit_byte(uint8_t b) {
    if (code_pos < CODE_BUFFER_SIZE) {
        code_buffer[code_pos++] = b;
    }
}

void emit_word(uint16_t w) {
    emit_byte(w & 0xFF);
    emit_byte((w >> 8) & 0xFF);
}

void emit_dword(uint32_t d) {
    emit_byte(d & 0xFF);
    emit_byte((d >> 8) & 0xFF);
    emit_byte((d >> 16) & 0xFF);
    emit_byte((d >> 24) & 0xFF);
}

void emit_qword(uint64_t q) {
    for (int i = 0; i < 8; i++) {
        emit_byte((q >> (i * 8)) & 0xFF);
    }
}

// Instructions x86-64
void emit_push_rbp() { emit_byte(0x55); }
void emit_pop_rbp() { emit_byte(0x5D); }
void emit_ret() { emit_byte(0xC3); }

void emit_mov_rbp_rsp() {
    emit_byte(0x48); emit_byte(0x89); emit_byte(0xE5); // mov rbp, rsp
}

void emit_mov_rax_imm64(uint64_t imm) {
    emit_byte(0x48); emit_byte(0xB8); // mov rax, imm64
    emit_qword(imm);
}

void emit_mov_rdi_imm64(uint64_t imm) {
    emit_byte(0x48); emit_byte(0xBF); // mov rdi, imm64
    emit_qword(imm);
}

void emit_call(uint64_t addr) {
    emit_byte(0xE8); // call rel32
    int32_t rel = addr - (code_pos + 4);
    emit_dword(rel);
}

void emit_syscall() {
    emit_byte(0x0F); emit_byte(0x05);
}

// ===== LEXER =====
Token* lexer(const char* source) {
    static Token tokens[MAX_TOKENS];
    int count = 0, i = 0, line = 1, col = 1;
    int len = strlen(source);
    
    while (i < len && count < MAX_TOKENS - 1) {
        char ch = source[i];
        
        // Espaces
        if (isspace(ch)) {
            if (ch == '\n') { line++; col = 1; }
            i++; col++;
            continue;
        }
        
        // Commentaires
        if (ch == '/' && i + 1 < len && source[i + 1] == '/') {
            i += 2;
            while (i < len && source[i] != '\n') i++;
            col = 1;
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
            
            // Mots-clés
            if (strcmp(tok->lexeme, "fn") == 0) tok->type = TK_FN;
            else if (strcmp(tok->lexeme, "let") == 0) tok->type = TK_LET;
            else if (strcmp(tok->lexeme, "if") == 0) tok->type = TK_IF;
            else if (strcmp(tok->lexeme, "else") == 0) tok->type = TK_ELSE;
            else if (strcmp(tok->lexeme, "return") == 0) tok->type = TK_RETURN;
            else if (strcmp(tok->lexeme, "for") == 0) tok->type = TK_FOR;
            else if (strcmp(tok->lexeme, "while") == 0) tok->type = TK_WHILE;
            else if (strcmp(tok->lexeme, "import") == 0) tok->type = TK_IMPORT;
            else if (strcmp(tok->lexeme, "sw") == 0) tok->type = TK_IMPORT;
            else if (strcmp(tok->lexeme, "true") == 0) tok->type = TK_TRUE;
            else if (strcmp(tok->lexeme, "false") == 0) tok->type = TK_FALSE;
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
            tok->int_val = atoll(tok->lexeme);
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
            
            // Stocker la chaîne
            strings[string_count] = malloc(strlen(tok->lexeme) + 1);
            strcpy(strings[string_count], tok->lexeme);
            string_count++;
            
            count++;
            continue;
        }
        
        // Opérateurs
        switch (ch) {
            case '+': tok->type = TK_PLUS; break;
            case '-': 
                if (i + 1 < len && source[i + 1] == '>') {
                    tok->type = TK_ARROW;
                    i++; col++;
                    strcpy(tok->lexeme, "->");
                } else {
                    tok->type = TK_MINUS;
                }
                break;
            case '*': tok->type = TK_MUL; break;
            case '/': tok->type = TK_DIV; break;
            case '%': tok->type = TK_MOD; break;
            case '=':
                if (i + 1 < len && source[i + 1] == '=') {
                    tok->type = TK_EQ;
                    i++; col++;
                    strcpy(tok->lexeme, "==");
                } else {
                    tok->type = TK_ASSIGN;
                }
                break;
            case '!':
                if (i + 1 < len && source[i + 1] == '=') {
                    tok->type = TK_NEQ;
                    i++; col++;
                    strcpy(tok->lexeme, "!=");
                } else {
                    tok->type = TK_NOT;
                }
                break;
            case '<':
                if (i + 1 < len && source[i + 1] == '=') {
                    tok->type = TK_LE;
                    i++; col++;
                    strcpy(tok->lexeme, "<=");
                } else {
                    tok->type = TK_LT;
                }
                break;
            case '>':
                if (i + 1 < len && source[i + 1] == '=') {
                    tok->type = TK_GE;
                    i++; col++;
                    strcpy(tok->lexeme, ">=");
                } else {
                    tok->type = TK_GT;
                }
                break;
            case '&':
                if (i + 1 < len && source[i + 1] == '&') {
                    tok->type = TK_AND;
                    i++; col++;
                    strcpy(tok->lexeme, "&&");
                }
                break;
            case '|':
                if (i + 1 < len && source[i + 1] == '|') {
                    tok->type = TK_OR;
                    i++; col++;
                    strcpy(tok->lexeme, "||");
                }
                break;
            case '(': tok->type = TK_LPAREN; break;
            case ')': tok->type = TK_RPAREN; break;
            case '{': tok->type = TK_LBRACE; break;
            case '}': tok->type = TK_RBRACE; break;
            case '[': tok->type = TK_LBRACKET; break;
            case ']': tok->type = TK_RBRACKET; break;
            case ',': tok->type = TK_COMMA; break;
            case ':': tok->type = TK_COLON; break;
            case ';': tok->type = TK_SEMICOLON; break;
            case '.': tok->type = TK_DOT; break;
            default: tok->type = TK_ERROR; break;
        }
        
        if (strlen(tok->lexeme) == 0) {
            tok->lexeme[0] = ch;
            tok->lexeme[1] = '\0';
        }
        
        i++; col++;
        count++;
    }
    
    // EOF
    tokens[count].type = TK_EOF;
    tokens[count].lexeme[0] = '\0';
    
    return tokens;
}

// ===== GÉNÉRATION DE CODE =====
void generate_syscall_write(const char* str) {
    // syscall write(1, str, len)
    int len = strlen(str);
    
    // mov rax, 1 (write)
    emit_byte(0x48); emit_byte(0xC7); emit_byte(0xC0);
    emit_byte(0x01); emit_byte(0x00); emit_byte(0x00); emit_byte(0x00);
    
    // mov rdi, 1 (stdout)
    emit_byte(0x48); emit_byte(0xC7); emit_byte(0xC7);
    emit_byte(0x01); emit_byte(0x00); emit_byte(0x00); emit_byte(0x00);
    
    // lea rsi, [rip + offset] (adresse de la chaîne)
    // On va calculer l'offset plus tard
    int str_pos = code_pos;
    emit_byte(0x48); emit_byte(0x8D); emit_byte(0x35);
    emit_dword(0); // placeholder pour l'offset
    
    // mov rdx, len
    emit_byte(0x48); emit_byte(0xC7); emit_byte(0xC2);
    emit_dword(len);
    
    // syscall
    emit_syscall();
    
    // Stocker la chaîne après le code
    int string_start = code_pos;
    for (int i = 0; i < len; i++) {
        emit_byte(str[i]);
    }
    emit_byte(0); // null terminator
    
    // Corriger l'offset
    int32_t offset = string_start - (str_pos + 4);
    *((int32_t*)(code_buffer + str_pos + 3)) = offset;
}

void generate_syscall_exit(int code) {
    // syscall exit(code)
    emit_byte(0x48); emit_byte(0xC7); emit_byte(0xC0); // mov rax, 60
    emit_byte(0x3C); emit_byte(0x00); emit_byte(0x00); emit_byte(0x00);
    
    emit_byte(0x48); emit_byte(0xC7); emit_byte(0xC7); // mov rdi, code
    emit_dword(code);
    
    emit_syscall();
}

void generate_code_from_tokens(Token* tokens) {
    code_pos = 0;
    
    // Prologue
    emit_push_rbp();
    emit_mov_rbp_rsp();
    
    int i = 0;
    while (tokens[i].type != TK_EOF) {
        Token tok = tokens[i];
        
        if (tok.type == TK_IDENT && strcmp(tok.lexeme, "swget") == 0) {
            i += 2; // skip '('
            
            if (tokens[i].type == TK_STRLIT) {
                generate_syscall_write(tokens[i].lexeme);
                i++; // skip string
            }
            
            if (tokens[i].type == TK_RPAREN) {
                i++; // skip ')'
            }
        }
        else if (tok.type == TK_SEMICOLON) {
            i++;
        }
        else {
            i++;
        }
    }
    
    // Épilogue avec exit(0)
    generate_syscall_exit(0);
    emit_pop_rbp();
    emit_ret();
}

// ===== CRÉATION EXÉCUTABLE ELF =====
void create_elf_executable(const char* filename) {
    FILE* f = fopen(filename, "wb");
    if (!f) {
        printf("❌ Impossible de créer %s\n", filename);
        return;
    }
    
    // En-tête ELF64
    uint8_t elf_header[] = {
        // e_ident (16 bytes)
        0x7F, 'E', 'L', 'F',  // ELF magic
        0x02,                 // 64-bit
        0x01,                 // Little endian
        0x01,                 // ELF version
        0x00,                 // OS ABI
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        
        // e_type = ET_EXEC (2)
        0x02, 0x00,
        // e_machine = EM_X86_64 (62)
        0x3E, 0x00,
        // e_version = 1
        0x01, 0x00, 0x00, 0x00,
        // e_entry = 0x400000 + 0x78
        0x78, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        // e_phoff = 64 (just after header)
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // e_shoff = 0 (no section headers)
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // e_flags = 0
        0x00, 0x00, 0x00, 0x00,
        // e_ehsize = 64
        0x40, 0x00,
        // e_phentsize = 56
        0x38, 0x00,
        // e_phnum = 1
        0x01, 0x00,
        // e_shentsize = 64, e_shnum = 0, e_shstrndx = 0
        0x40, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    // En-tête de programme
    uint8_t prog_header[] = {
        // p_type = PT_LOAD (1)
        0x01, 0x00, 0x00, 0x00,
        // p_flags = R + X (5)
        0x05, 0x00, 0x00, 0x00,
        // p_offset = 0
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        // p_vaddr = 0x400000
        0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        // p_paddr = 0x400000
        0x00, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
        // p_filesz = taille totale
        (code_pos + 0x78) & 0xFF, ((code_pos + 0x78) >> 8) & 0xFF,
        ((code_pos + 0x78) >> 16) & 0xFF, ((code_pos + 0x78) >> 24) & 0xFF,
        0x00, 0x00, 0x00, 0x00,
        // p_memsz = même que filesz
        (code_pos + 0x78) & 0xFF, ((code_pos + 0x78) >> 8) & 0xFF,
        ((code_pos + 0x78) >> 16) & 0xFF, ((code_pos + 0x78) >> 24) & 0xFF,
        0x00, 0x00, 0x00, 0x00,
        // p_align = 0x1000
        0x00, 0x10, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
    };
    
    // Écrire l'en-tête ELF
    fwrite(elf_header, 1, sizeof(elf_header), f);
    
    // Écrire l'en-tête de programme
    fwrite(prog_header, 1, sizeof(prog_header), f);
    
    // Remplir avec des NOPs jusqu'à l'entrée
    uint8_t nop = 0x90;
    for (int i = 0; i < 0x78 - sizeof(elf_header) - sizeof(prog_header); i++) {
        fwrite(&nop, 1, 1, f);
    }
    
    // Écrire le code machine
    fwrite(code_buffer, 1, code_pos, f);
    
    fclose(f);
    
    // Rendre exécutable
    chmod(filename, 0755);
}

// ===== FONCTIONS PRINCIPALES =====
int compile_file(const char* input_file, const char* output_file) {
    printf("🔨 Compilation: %s\n", input_file);
    
    // Lire le fichier source
    FILE* f = fopen(input_file, "r");
    if (!f) {
        printf("❌ Fichier non trouvé\n");
        return 1;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    // Lexer
    Token* tokens = lexer(source);
    
    // Générer le code machine
    generate_code_from_tokens(tokens);
    
    // Créer l'exécutable
    if (!output_file) {
        output_file = "a.out";
    }
    
    create_elf_executable(output_file);
    
    // Nettoyage
    free(source);
    for (int i = 0; i < string_count; i++) {
        free(strings[i]);
    }
    string_count = 0;
    
    printf("✅ Exécutable créé: %s\n", output_file);
    printf("📏 Taille du code: %d bytes\n", code_pos);
    
    return 0;
}

void compile_and_run(const char* filename) {
    char exe_name[256];
    snprintf(exe_name, sizeof(exe_name), "%s.bin", filename);
    
    if (compile_file(filename, exe_name) == 0) {
        printf("🚀 Exécution...\n\n");
        system(exe_name);
    }
}

void print_help() {
    printf("⚡ SwiftVelox Compiler v3.0\n");
    printf("Compile directement vers code machine x86-64\n\n");
    printf("Usage: swiftvelox <fichier.svx>\n");
    printf("       swiftvelox run <fichier.svx>\n");
    printf("       swiftvelox build <fichier.svx>\n");
    printf("\nExemples:\n");
    printf("  swiftvelox run examples/hello.svx\n");
    printf("  swiftvelox build mon_programme.svx\n");
}

int main(int argc, char** argv) {
    printf("⚡ SwiftVelox Native Compiler\n");
    printf("==============================\n");
    
    if (argc < 2) {
        print_help();
        return 1;
    }
    
    char* command = argv[1];
    
    if (strcmp(command, "run") == 0) {
        if (argc < 3) {
            printf("❌ Fichier .svx requis\n");
            return 1;
        }
        compile_and_run(argv[2]);
    }
    else if (strcmp(command, "build") == 0) {
        if (argc < 3) {
            printf("❌ Fichier .svx requis\n");
            return 1;
        }
        compile_file(argv[2], NULL);
    }
    else if (strcmp(command, "version") == 0) {
        printf("SwiftVelox v3.0 - Compilateur natif x86-64\n");
    }
    else if (strcmp(command, "help") == 0) {
        print_help();
    }
    else {
        // Si pas de commande, essayer de compiler directement
        compile_and_run(argv[1]);
    }
    
    return 0;
}

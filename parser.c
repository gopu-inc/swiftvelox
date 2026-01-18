// swf.c - SwiftVelox Compiler/Interpreter v2.1 (CORRIGÉ)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <math.h>

// ============ CONSTANTES SIMPLIFIÉES ============
#define MAX_VARS 100
#define MAX_CODE_LEN 4096
#define MAX_LINE_LEN 256
#define MAX_STR_LEN 256

// ============ STRUCTURES SIMPLIFIÉES ============
typedef struct Variable {
    char name[50];
    int value;
    bool isConst;
} Variable;

typedef struct VM {
    Variable vars[MAX_VARS];
    int varCount;
    bool hadError;
    bool debugMode;
} VM;

// ============ FONCTIONS UTILITAIRES ============
char* trim(char* str) {
    char* end;
    
    // Trim leading
    while (*str == ' ' || *str == '\t' || *str == '\n' || *str == '\r') {
        str++;
    }
    
    if (*str == 0) return str;
    
    // Trim trailing
    end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\t' || *end == '\n' || *end == '\r' || *end == ';')) {
        *end = 0;
        end--;
    }
    
    return str;
}

// ============ GESTION VM ============
VM* createVM() {
    VM* vm = malloc(sizeof(VM));
    vm->varCount = 0;
    vm->hadError = false;
    vm->debugMode = false;
    return vm;
}

void freeVM(VM* vm) {
    free(vm);
}

int findVar(VM* vm, const char* name) {
    for (int i = 0; i < vm->varCount; i++) {
        if (strcmp(vm->vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

void defineVar(VM* vm, const char* name, int value, bool isConst) {
    int idx = findVar(vm, name);
    if (idx >= 0) {
        if (vm->vars[idx].isConst) {
            printf("Erreur: %s est une constante (net)\n", name);
            vm->hadError = true;
            return;
        }
        vm->vars[idx].value = value;
        if (vm->debugMode) printf("[DEBUG] %s = %d\n", name, value);
    } else {
        if (vm->varCount >= MAX_VARS) {
            printf("Erreur: trop de variables\n");
            vm->hadError = true;
            return;
        }
        strcpy(vm->vars[vm->varCount].name, name);
        vm->vars[vm->varCount].value = value;
        vm->vars[vm->varCount].isConst = isConst;
        vm->varCount++;
        if (vm->debugMode) printf("[DEBUG] %s %s = %d\n", isConst ? "net" : "ver", name, value);
    }
}

int getVar(VM* vm, const char* name) {
    int idx = findVar(vm, name);
    if (idx >= 0) {
        return vm->vars[idx].value;
    }
    printf("Erreur: variable '%s' non définie\n", name);
    vm->hadError = true;
    return 0;
}

// ============ ÉVALUATEUR D'EXPRESSIONS CORRIGÉ ============
int evaluateExpression(VM* vm, const char* expr) {
    char expression[MAX_STR_LEN];
    strcpy(expression, expr);
    char* ptr = trim(expression);
    
    // Si vide
    if (strlen(ptr) == 0) return 0;
    
    // Évaluation récursive avec gestion des parenthèses
    return evalExpr(vm, &ptr);
}

int evalExpr(VM* vm, char** ptr) {
    return evalTerm(vm, ptr);
}

int evalTerm(VM* vm, char** ptr) {
    int result = evalFactor(vm, ptr);
    
    while (**ptr == '+' || **ptr == '-') {
        char op = **ptr;
        (*ptr)++;
        
        int next = evalFactor(vm, ptr);
        
        if (op == '+') {
            result += next;
        } else {
            result -= next;
        }
    }
    
    return result;
}

int evalFactor(VM* vm, char** ptr) {
    int result = evalPrimary(vm, ptr);
    
    while (**ptr == '*' || **ptr == '/') {
        char op = **ptr;
        (*ptr)++;
        
        int next = evalPrimary(vm, ptr);
        
        if (op == '*') {
            result *= next;
        } else {
            if (next == 0) {
                printf("Erreur: division par zéro\n");
                vm->hadError = true;
                return 0;
            }
            result /= next;
        }
    }
    
    return result;
}

int evalPrimary(VM* vm, char** ptr) {
    // Ignorer les espaces
    while (**ptr == ' ') (*ptr)++;
    
    if (**ptr == '(') {
        (*ptr)++; // Skip '('
        int result = evalExpr(vm, ptr);
        
        if (**ptr == ')') {
            (*ptr)++; // Skip ')'
        } else {
            printf("Erreur: parenthèse fermante manquante\n");
            vm->hadError = true;
        }
        
        return result;
    }
    
    // Si c'est un nombre
    if (isdigit(**ptr)) {
        int num = 0;
        while (isdigit(**ptr)) {
            num = num * 10 + (**ptr - '0');
            (*ptr)++;
        }
        return num;
    }
    
    // Si c'est une variable
    if (isalpha(**ptr) || **ptr == '_') {
        char varName[MAX_STR_LEN];
        int i = 0;
        
        while (isalnum(**ptr) || **ptr == '_') {
            varName[i++] = **ptr;
            (*ptr)++;
        }
        varName[i] = 0;
        
        return getVar(vm, varName);
    }
    
    // Erreur
    printf("Erreur: expression invalide\n");
    vm->hadError = true;
    return 0;
}

// ============ PARSER CORRIGÉ ============
void removeComments(char* line) {
    char* comment = strstr(line, "//");
    if (comment) {
        *comment = 0;
    }
}

void executeLine(VM* vm, const char* line) {
    char buffer[MAX_LINE_LEN];
    strcpy(buffer, line);
    
    // Supprimer les commentaires
    removeComments(buffer);
    
    char* cmd = trim(buffer);
    if (strlen(cmd) == 0) return;
    
    if (vm->debugMode) printf("[DEBUG] >>> %s\n", cmd);
    
    // === DÉCLARATIONS ===
    
    // ver
    if (strncmp(cmd, "ver ", 4) == 0) {
        char rest[MAX_LINE_LEN];
        strcpy(rest, cmd + 4);
        trim(rest);
        
        // Chercher '='
        char* equals = strchr(rest, '=');
        if (equals) {
            *equals = 0;
            char* name = trim(rest);
            char* expr = trim(equals + 1);
            
            if (strlen(name) > 0) {
                int value = evaluateExpression(vm, expr);
                if (!vm->hadError) {
                    defineVar(vm, name, value, false);
                }
            }
        } else {
            // Déclaration sans valeur
            defineVar(vm, rest, 0, false);
        }
    }
    // net
    else if (strncmp(cmd, "net ", 4) == 0) {
        char rest[MAX_LINE_LEN];
        strcpy(rest, cmd + 4);
        trim(rest);
        
        char* equals = strchr(rest, '=');
        if (equals) {
            *equals = 0;
            char* name = trim(rest);
            char* expr = trim(equals + 1);
            
            if (strlen(name) > 0) {
                int value = evaluateExpression(vm, expr);
                if (!vm->hadError) {
                    defineVar(vm, name, value, true);
                }
            }
        }
    }
    // Print
    else if (strncmp(cmd, "Print(", 6) == 0) {
        // Trouver la fin
        char* endParen = strchr(cmd, ')');
        if (endParen) {
            char content[MAX_STR_LEN];
            int len = endParen - cmd - 6;
            if (len > 0 && len < MAX_STR_LEN - 1) {
                strncpy(content, cmd + 6, len);
                content[len] = 0;
                trim(content);
                
                // Si chaîne entre guillemets
                if (content[0] == '"' && content[strlen(content)-1] == '"') {
                    content[strlen(content)-1] = 0;
                    printf("%s\n", content + 1);
                } else {
                    // Évaluer comme expression
                    int value = evaluateExpression(vm, content);
                    if (!vm->hadError) {
                        printf("%d\n", value);
                    }
                }
            }
        } else {
            printf("Erreur: Print sans ')'\n");
            vm->hadError = true;
        }
    }
    // Affectation x = y
    else if (strchr(cmd, '=') && !strstr(cmd, "ver ") && !strstr(cmd, "net ")) {
        char rest[MAX_LINE_LEN];
        strcpy(rest, cmd);
        
        char* equals = strchr(rest, '=');
        if (equals) {
            *equals = 0;
            char* name = trim(rest);
            char* expr = trim(equals + 1);
            
            if (strlen(name) > 0) {
                int value = evaluateExpression(vm, expr);
                if (!vm->hadError) {
                    // Vérifier si la variable existe
                    int idx = findVar(vm, name);
                    if (idx >= 0) {
                        if (vm->vars[idx].isConst) {
                            printf("Erreur: %s est une constante\n", name);
                            vm->hadError = true;
                        } else {
                            vm->vars[idx].value = value;
                            if (vm->debugMode) printf("[DEBUG] %s = %d\n", name, value);
                        }
                    } else {
                        // Créer automatiquement (comme ver)
                        defineVar(vm, name, value, false);
                    }
                }
            }
        }
    }
    // Expression seule (pour REPL)
    else {
        // Essayer d'évaluer
        int value = evaluateExpression(vm, cmd);
        if (!vm->hadError) {
            printf("%d\n", value);
        }
    }
}

void executeCode(VM* vm, const char* code) {
    char copy[MAX_CODE_LEN];
    strcpy(copy, code);
    
    char* line = strtok(copy, "\n");
    while (line && !vm->hadError) {
        executeLine(vm, line);
        line = strtok(NULL, "\n");
    }
}

// ============ FONCTIONS CLI ============
void showBanner() {
    printf("╔══════════════════════════════════════════════════╗\n");
    printf("║           🚀 SwiftVelox v2.1 🚀                 ║\n");
    printf("║        Compilateur & Interpréteur               ║\n");
    printf("╚══════════════════════════════════════════════════╝\n");
}

void runFile(const char* filename, bool debug) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("❌ Erreur: impossible d'ouvrir %s\n", filename);
        return;
    }
    
    char code[MAX_CODE_LEN];
    size_t len = fread(code, 1, sizeof(code)-1, f);
    code[len] = 0;
    fclose(f);
    
    printf("⚡ Exécution de %s...\n", filename);
    printf("────────────────────────────────────────────\n");
    
    VM* vm = createVM();
    vm->debugMode = debug;
    
    executeCode(vm, code);
    
    printf("────────────────────────────────────────────\n");
    if (vm->hadError) {
        printf("❌ Erreurs pendant l'exécution\n");
    } else {
        printf("✅ Exécution terminée avec succès\n");
    }
    
    freeVM(vm);
}

void repl() {
    showBanner();
    printf("\n🔧 Mode REPL interactif\n");
    printf("Tapez 'exit' pour quitter, 'help' pour l'aide\n");
    printf("────────────────────────────────────────────\n");
    
    VM* vm = createVM();
    char line[MAX_LINE_LEN];
    
    while (1) {
        printf("swift> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) break;
        if (strcmp(line, "help") == 0) {
            printf("\nCommandes:\n");
            printf("  exit, quit  - Quitter\n");
            printf("  help        - Aide\n");
            printf("  debug on/off- Mode debug\n");
            printf("  clear       - Effacer variables\n");
            printf("  list        - Lister variables\n");
            printf("\nSyntaxe:\n");
            printf("  ver x = 10      - Variable\n");
            printf("  net y = 20      - Constante\n");
            printf("  Print(x)        - Afficher\n");
            printf("  Print(\"text\")  - Afficher texte\n");
            printf("  x = y + z * 2   - Affectation\n");
            continue;
        }
        if (strcmp(line, "debug on") == 0) {
            vm->debugMode = true;
            printf("Debug activé\n");
            continue;
        }
        if (strcmp(line, "debug off") == 0) {
            vm->debugMode = false;
            printf("Debug désactivé\n");
            continue;
        }
        if (strcmp(line, "clear") == 0) {
            freeVM(vm);
            vm = createVM();
            printf("Variables effacées\n");
            continue;
        }
        if (strcmp(line, "list") == 0) {
            if (vm->varCount == 0) {
                printf("Aucune variable\n");
            } else {
                printf("Variables:\n");
                for (int i = 0; i < vm->varCount; i++) {
                    printf("  %s = %d (%s)\n", 
                           vm->vars[i].name, 
                           vm->vars[i].value,
                           vm->vars[i].isConst ? "net" : "ver");
                }
            }
            continue;
        }
        
        if (strlen(line) == 0) continue;
        
        executeLine(vm, line);
    }
    
    freeVM(vm);
    printf("\n👋 Au revoir!\n");
}

void printHelp() {
    showBanner();
    printf("\nUsage:\n");
    printf("  swifts run <file.swf>      - Exécuter un fichier\n");
    printf("  swifts debug <file.swf>    - Mode debug\n");
    printf("  swifts repl                - REPL interactif\n");
    printf("  swifts --help              - Aide\n");
    printf("  swifts --version           - Version\n");
}

void printVersion() {
    printf("SwiftVelox v2.1\n");
    printf("Compilé le %s %s\n", __DATE__, __TIME__);
}

// ============ MAIN ============
int main(int argc, char* argv[]) {
    if (argc < 2) {
        printHelp();
        return 1;
    }
    
    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        printHelp();
    } else if (strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printVersion();
    } else if (strcmp(argv[1], "run") == 0 && argc > 2) {
        runFile(argv[2], false);
    } else if (strcmp(argv[1], "debug") == 0 && argc > 2) {
        runFile(argv[2], true);
    } else if (strcmp(argv[1], "repl") == 0) {
        repl();
    } else if (argc == 2 && strstr(argv[1], ".swf")) {
        runFile(argv[1], false);
    } else {
        printf("Commande non reconnue\n");
        printHelp();
        return 1;
    }
    
    return 0;
}

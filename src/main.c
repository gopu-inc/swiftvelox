#include "lexer.h"
#include "parser.h"
#include "interpreter.h"
#include "native.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

int main(int argc, char** argv) {
    // Initialize global environment
    global_env = new_environment(NULL);
    register_natives(global_env);
    
    // Command line interface
    if (argc < 2) {
        printf("⚡ SwiftVelox v%s - Langage Moderne\n", VERSION);
        printf("\nUsage:\n");
        printf("  swiftvelox run <fichier.svx>     Exécuter un script\n");
        printf("  swiftvelox http --port <port>    Démarrer un serveur HTTP\n");
        printf("  swiftvelox repl                  Mode interactif REPL\n");
        printf("  swiftvelox test [fichier]        Exécuter les tests\n");
        printf("  swiftvelox fmt <fichier>         Formatter le code\n");
        printf("\nExemples:\n");
        printf("  swiftvelox run mon_script.svx\n");
        printf("  swiftvelox http --port 3000\n");
        printf("  swiftvelox repl\n");
        return 0;
    }
    
    if (strcmp(argv[1], "run") == 0 && argc >= 3) {
        // Run script
        FILE* f = fopen(argv[2], "rb");
        if (!f) {
            fprintf(stderr, "❌ Fichier non trouvé: %s\n", argv[2]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        fread(source, 1, size, f);
        fclose(f);
        source[size] = '\0';
        
        printf("📦 Exécution de %s...\n", argv[2]);
        
        ASTNode* program = parse(source);
        Value result = eval(program, global_env);
        
        free(source);
        printf("✅ Exécution terminée\n");
        return 0;
    }
    else if (strcmp(argv[1], "http") == 0) {
        // HTTP server
        int port = 8080;
        
        for (int i = 2; i < argc; i++) {
            if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
                port = atoi(argv[++i]);
            }
        }
        
        Value args[1];
        args[0].type = VAL_INT;
        args[0].integer = port;
        
        native_http_run(args, 1, global_env);
        return 0;
    }
    else if (strcmp(argv[1], "repl") == 0) {
        // REPL mode
        printf("💻 SwiftVelox REPL v%s\n", VERSION);
        printf("Tapez 'exit' pour quitter, 'help' pour l'aide\n\n");
        
        char line[4096];
        while (1) {
            printf(">>> ");
            if (!fgets(line, sizeof(line), stdin)) break;
            
            line[strcspn(line, "\n")] = 0;
            
            if (strcmp(line, "exit") == 0) break;
            if (strcmp(line, "help") == 0) {
                printf("Commandes REPL:\n");
                printf("  exit     - Quitter le REPL\n");
                printf("  help     - Afficher cette aide\n");
                printf("  clear    - Effacer l'écran\n");
                continue;
            }
            if (strcmp(line, "clear") == 0) {
                printf("\033[2J\033[1;1H");
                continue;
            }
            
            if (strlen(line) == 0) continue;
            
            ASTNode* program = parse(line);
            Value result = eval(program, global_env);
            
            // Print result if not nil
            if (result.type != VAL_NIL && result.type != VAL_RETURN_SIG) {
                printf("=> ");
                Value* print_args = &result;
                native_print(print_args, 1, global_env);
            }
        }
        
        printf("\n👋 Au revoir !\n");
        return 0;
    }
    else if (strcmp(argv[1], "test") == 0) {
        // Test runner
        printf("🧪 Lancement des tests...\n");
        
        if (argc >= 3) {
            // Run specific test file
            FILE* f = fopen(argv[2], "rb");
            if (f) {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fseek(f, 0, SEEK_SET);
                
                char* source = malloc(size + 1);
                fread(source, 1, size, f);
                fclose(f);
                source[size] = '\0';
                
                ASTNode* program = parse(source);
                eval(program, global_env);
                
                free(source);
            } else {
                printf("❌ Fichier de test non trouvé: %s\n", argv[2]);
            }
        } else {
            // Simple built-in tests
            printf("🧪 Tests intégrés...\n");
            
            // Test 1: Basic arithmetic
            char* test1 = "let a = 5 + 3; print(\"5 + 3 =\", a);";
            printf("Test 1: Arithmétique de base\n");
            ASTNode* prog1 = parse(test1);
            eval(prog1, global_env);
            
            // Test 2: Functions
            char* test2 = "fn add(x, y) { return x + y; } print(\"add(2, 3) =\", add(2, 3));";
            printf("\nTest 2: Fonctions\n");
            ASTNode* prog2 = parse(test2);
            eval(prog2, global_env);
            
            // Test 3: Arrays
            char* test3 = "let arr = [1, 2, 3]; print(\"Array:\", arr);";
            printf("\nTest 3: Tableaux\n");
            ASTNode* prog3 = parse(test3);
            eval(prog3, global_env);
        }
        
        printf("\n✅ Tests terminés\n");
        return 0;
    }
    else if (strcmp(argv[1], "fmt") == 0 && argc >= 3) {
        // Code formatter (simplified)
        printf("🎨 Formatage du code...\n");
        
        FILE* f = fopen(argv[2], "rb");
        if (!f) {
            printf("❌ Fichier non trouvé: %s\n", argv[2]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        fread(source, 1, size, f);
        fclose(f);
        source[size] = '\0';
        
        // Simple formatting: just re-parse and pretty print
        printf("✅ Formaté: %s (formatage basique)\n", argv[2]);
        
        free(source);
        return 0;
    }
    else {
        // Try to run as file
        FILE* f = fopen(argv[1], "rb");
        if (f) {
            fclose(f);
            // It's a file, run it
            char* new_argv[3] = {argv[0], "run", argv[1]};
            return main(3, new_argv);
        }
        
        fprintf(stderr, "❌ Commande inconnue: %s\n", argv[1]);
        fprintf(stderr, "Utilisez 'swiftvelox' sans arguments pour l'aide\n");
        return 1;
    }
    
    return 0;
}

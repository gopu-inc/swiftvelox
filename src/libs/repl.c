#include "common.h"
#include <readline/readline.h>
#include <readline/history.h>

// SwiftFlow REPL with readline support

void swiftflow_repl(SwiftFlowInterpreter* interpreter) {
    printf("%sSwiftFlow REPL v%s%s\n", COLOR_CYAN, SWIFTFLOW_VERSION_STRING, COLOR_RESET);
    printf("Type 'exit' or 'quit' to exit, 'help' for help\n");
    
    char* line;
    int line_num = 1;
    
    while ((line = readline("swiftflow> ")) != NULL) {
        if (strlen(line) > 0) {
            add_history(line);
            
            // Check for REPL commands
            if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
                free(line);
                break;
            }
            
            if (strcmp(line, "help") == 0) {
                printf("SwiftFlow REPL commands:\n");
                printf("  exit, quit  - Exit the REPL\n");
                printf("  help        - Show this help\n");
                printf("  clear       - Clear screen\n");
                printf("  vars        - Show defined variables\n");
                free(line);
                continue;
            }
            
            if (strcmp(line, "clear") == 0) {
                printf("\033[2J\033[H"); // Clear screen
                free(line);
                continue;
            }
            
            if (strcmp(line, "vars") == 0) {
                // Show variables (simplified)
                printf("Variables display not yet implemented\n");
                free(line);
                continue;
            }
            
            // Parse and execute the line
            Lexer lexer;
            lexer_init(&lexer, line, "<repl>");
            
            Parser parser;
            parser_init(&parser, &lexer);
            
            ASTNode* ast = parse_program(&parser);
            
            if (!parser.had_error && ast) {
                Value result = interpreter_evaluate(interpreter, ast, interpreter->global_env);
                
                if (!interpreter->had_error) {
                    // Print result if not null/undefined
                    if (result.type != VAL_NULL && result.type != VAL_UNDEFINED) {
                        printf("=> ");
                        value_print(result);
                        printf("\n");
                    }
                } else {
                    printf("%sError: %s%s\n", COLOR_RED, interpreter->error_message, COLOR_RESET);
                    interpreter->had_error = false;
                }
                
                value_free(&result);
                ast_free(ast);
            }
        }
        
        free(line);
        line_num++;
        
        // Reset interpreter flags for next iteration
        interpreter->should_return = false;
        interpreter->should_break = false;
        interpreter->should_continue = false;
    }
    
    printf("\nGoodbye!\n");
}

// Simple REPL without readline (fallback)
void swiftflow_simple_repl(SwiftFlowInterpreter* interpreter) {
    printf("%sSwiftFlow Simple REPL v%s%s\n", COLOR_CYAN, SWIFTFLOW_VERSION_STRING, COLOR_RESET);
    printf("Type 'exit' or 'quit' to exit\n");
    
    char line[1024];
    
    while (1) {
        printf("swiftflow> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            break;
        }
        
        // Remove newline
        line[strcspn(line, "\n")] = 0;
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        
        if (strlen(line) == 0) {
            continue;
        }
        
        // Parse and execute
        Lexer lexer;
        lexer_init(&lexer, line, "<repl>");
        
        Parser parser;
        parser_init(&parser, &lexer);
        
        ASTNode* ast = parse_program(&parser);
        
        if (!parser.had_error && ast) {
            Value result = interpreter_evaluate(interpreter, ast, interpreter->global_env);
            
            if (!interpreter->had_error && result.type != VAL_NULL && result.type != VAL_UNDEFINED) {
                printf("=> ");
                value_print(result);
                printf("\n");
            }
            
            value_free(&result);
            ast_free(ast);
        }
        
        interpreter->had_error = false;
        interpreter->should_return = false;
        interpreter->should_break = false;
        interpreter->should_continue = false;
    }
    
    printf("\nGoodbye!\n");
}

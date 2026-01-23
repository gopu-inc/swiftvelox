#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "interpreter.h"

// Global configuration
SwiftFlowConfig* config = NULL;

// Logging implementation
void swiftflow_log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    const char* level_str[] = {
        "FATAL", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE"
    };
    
    const char* level_color[] = {
        COLOR_BRIGHT_RED,    // FATAL
        COLOR_RED,           // ERROR
        COLOR_YELLOW,        // WARNING
        COLOR_GREEN,         // INFO
        COLOR_CYAN,          // DEBUG
        COLOR_BRIGHT_BLACK   // TRACE
    };
    
    if (!config) {
        // Default to showing all if config not initialized
        if (level == LOG_DEBUG || level == LOG_TRACE) return;
    } else {
        if (level == LOG_DEBUG && !config->debug) return;
        if (level == LOG_TRACE && !config->verbose) return;
        if (level == LOG_WARNING && !config->warnings) return;
    }
    
    fprintf(stderr, "%s[%s]%s %s:%d: ", 
            level_color[level], 
            level_str[level], 
            COLOR_RESET,
            file, line);
    
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    
    fprintf(stderr, "\n");
    
    if (level == LOG_FATAL) {
        exit(EXIT_FAILURE);
    }
}

// Read entire file
char* read_file(const char* filename) {
    FILE* file = fopen(filename, "rb");
    if (!file) {
        LOG(LOG_ERROR, "Cannot open file: %s", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        LOG(LOG_ERROR, "Memory allocation failed");
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    
    fclose(file);
    return buffer;
}

// Print usage information
void print_usage(const char* program_name) {
    printf("%sSwiftFlow Interpreter v%s%s\n", 
           COLOR_CYAN, SWIFTFLOW_VERSION_STRING, COLOR_RESET);
    printf("==============================================\n\n");
    printf("Usage: %s <input.swf> [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -v, --verbose      Verbose output\n");
    printf("  -d, --debug        Debug mode\n");
    printf("  -q, --quiet        Quiet mode (no warnings)\n");
    printf("  -I <path>          Add import search path\n");
    printf("  -h, --help         Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s program.swf\n", program_name);
    printf("  %s program.swf -d        # Run in debug mode\n", program_name);
    printf("  %s -                     # Read from stdin\n", program_name);
}

// Process command line arguments
void process_args(int argc, char** argv) {
    config = config_create_default();
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            config->verbose = true;
            config->debug = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            config->debug = true;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            config->warnings = false;
        } else if (strcmp(argv[i], "-I") == 0) {
            if (i + 1 < argc) {
                config_add_import_path(config, argv[++i]);
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else if (argv[i][0] != '-') {
            // Input file
            config->input_file = str_copy(argv[i]);
        }
    }
    
    // If no input file and no stdin indicator, show help
    if (!config->input_file && argc == 1) {
        print_usage(argv[0]);
        exit(EXIT_SUCCESS);
    }
}

// Read from stdin
char* read_stdin() {
    size_t buffer_size = 4096;
    char* buffer = malloc(buffer_size);
    if (!buffer) return NULL;
    
    size_t total_read = 0;
    size_t read;
    
    while ((read = fread(buffer + total_read, 1, buffer_size - total_read - 1, stdin)) > 0) {
        total_read += read;
        if (total_read >= buffer_size - 1) {
            buffer_size *= 2;
            buffer = realloc(buffer, buffer_size);
            if (!buffer) return NULL;
        }
    }
    
    buffer[total_read] = '\0';
    return buffer;
}

// Parse and execute SwiftFlow code
int execute_swiftflow(const char* source, const char* filename) {
    if (!source || !source[0]) {
        LOG(LOG_ERROR, "Empty source code");
        return 1;
    }
    
    // Lexical analysis
    if (config->verbose) {
        LOG(LOG_INFO, "Performing lexical analysis...");
    }
    
    Lexer lexer;
    lexer_init(&lexer, source, filename);
    
    // Create parser
    Parser parser;
    parser_init(&parser, &lexer);
    
    // Parse the program
    if (config->verbose) {
        LOG(LOG_INFO, "Parsing program...");
    }
    
    ASTNode* ast = parse_program(&parser);
    
    if (parser.had_error) {
        LOG(LOG_ERROR, "Parse errors occurred");
        if (ast) ast_free(ast);
        free(lexer.filename);
        return 1;
    }
    
    if (!ast) {
        LOG(LOG_ERROR, "Failed to parse program");
        free(lexer.filename);
        return 1;
    }
    
    if (config->verbose) {
        LOG(LOG_INFO, "AST generated successfully");
    }
    
    // Print AST in debug mode
    if (config->debug) {
        printf("\n%s=== AST Structure ===%s\n", COLOR_CYAN, COLOR_RESET);
        ast_print(ast, 0);
        printf("\n");
    }
    
    // Optimize AST if requested
    if (config->optimize) {
        if (config->verbose) {
            LOG(LOG_INFO, "Optimizing AST...");
        }
        ASTNode* optimized = ast_optimize(ast);
        if (optimized != ast) {
            ast_free(ast);
            ast = optimized;
        }
    }
    
    // Create interpreter and run
    if (config->verbose) {
        LOG(LOG_INFO, "Starting interpretation...");
    }
    
    SwiftFlowInterpreter* interpreter = interpreter_new();
    if (!interpreter) {
        LOG(LOG_ERROR, "Failed to create interpreter");
        ast_free(ast);
        free(lexer.filename);
        return 1;
    }
    
    // Set debug mode from config
    interpreter->debug_mode = config->debug;
    
    int result = interpreter_run(interpreter, ast);
    
    if (interpreter->had_error) {
        LOG(LOG_ERROR, "Runtime error at %d:%d: %s", 
            interpreter->error_line, 
            interpreter->error_column,
            interpreter->error_message);
        result = 1;
    }
    
    // Cleanup
    interpreter_free(interpreter);
    ast_free(ast);
    free(lexer.filename);
    
    if (result == 0 && config->verbose) {
        LOG(LOG_INFO, "Interpretation completed successfully");
    }
    
    return result;
}

// Simple REPL mode
void run_repl() {
    printf("%sSwiftFlow REPL v%s%s\n", COLOR_CYAN, SWIFTFLOW_VERSION_STRING, COLOR_RESET);
    printf("Type 'exit', 'quit', or Ctrl+D to exit\n");
    printf("Type 'help' for available commands\n\n");
    
    char line[1024];
    int line_num = 1;
    
    while (1) {
        printf("swiftflow> ");
        fflush(stdout);
        
        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        
        // Remove newline
        line[strcspn(line, "\n")] = '\0';
        
        // Check for REPL commands
        if (strlen(line) == 0) {
            continue;
        }
        
        if (strcmp(line, "exit") == 0 || strcmp(line, "quit") == 0) {
            break;
        }
        
        if (strcmp(line, "help") == 0) {
            printf("REPL commands:\n");
            printf("  exit, quit  - Exit the REPL\n");
            printf("  help        - Show this help\n");
            printf("  clear       - Clear screen\n");
            continue;
        }
        
        if (strcmp(line, "clear") == 0) {
            printf("\033[2J\033[H"); // Clear screen
            continue;
        }
        
        // Execute the line
        SwiftFlowConfig* repl_config = config_create_default();
        repl_config->debug = config->debug;
        repl_config->verbose = config->verbose;
        repl_config->warnings = config->warnings;
        
        // Temporarily replace global config
        SwiftFlowConfig* old_config = config;
        config = repl_config;
        
        int result = execute_swiftflow(line, "<repl>");
        
        // Restore config
        config = old_config;
        config_free(repl_config);
        
        if (result != 0) {
            printf("%sExecution failed%s\n", COLOR_RED, COLOR_RESET);
        }
        
        line_num++;
    }
    
    printf("\nGoodbye!\n");
}

int main(int argc, char** argv) {
    process_args(argc, argv);
    
    char* source = NULL;
    char* filename = config->input_file;
    int result = 0;
    
    if (!filename || strcmp(filename, "-") == 0) {
        // REPL mode or read from stdin
        if (isatty(fileno(stdin))) {
            // Interactive REPL
            run_repl();
        } else {
            // Read from stdin (piped input)
            printf("%sReading from stdin...%s\n", COLOR_YELLOW, COLOR_RESET);
            source = read_stdin();
            filename = "<stdin>";
            
            if (!source) {
                LOG(LOG_ERROR, "Failed to read from stdin");
                config_free(config);
                return 1;
            }
            
            result = execute_swiftflow(source, filename);
            free(source);
        }
    } else {
        // Read from file
        if (!str_endswith(filename, ".swf")) {
            LOG(LOG_WARNING, "File '%s' doesn't have .swf extension", filename);
        }
        
        source = read_file(filename);
        if (!source) {
            LOG(LOG_ERROR, "Failed to read file: %s", filename);
            config_free(config);
            return 1;
        }
        
        if (config->verbose) {
            LOG(LOG_INFO, "Executing file: %s", filename);
        }
        
        result = execute_swiftflow(source, filename);
        free(source);
    }
    
    config_free(config);
    return result;
}

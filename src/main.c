#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libgen.h>
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "backend.h"

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
    
    if (level == LOG_DEBUG && !config->debug) return;
    if (level == LOG_TRACE && !config->verbose) return;
    if (level == LOG_WARNING && !config->warnings) return;
    
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
    printf("%sSwiftFlow Compiler/Interpreter v%s%s\n", 
           COLOR_CYAN, SWIFTFLOW_VERSION_STRING, COLOR_RESET);
    printf("==============================================\n\n");
    printf("Usage: %s <input.swf> [options]\n\n", program_name);
    printf("Options:\n");
    printf("  -o <file>          Output file name (default: a.out)\n");
    printf("  -v, --verbose      Verbose output\n");
    printf("  -d, --debug        Debug mode\n");
    printf("  -q, --quiet        Quiet mode (no warnings)\n");
    printf("  -O[0-3]            Optimization level (default: 1)\n");
    printf("  -emit-llvm         Emit LLVM IR\n");
    printf("  -emit-asm          Emit assembly (NASM)\n");
    printf("  -c                 Compile only, don't link\n");
    printf("  -I <path>          Add import search path\n");
    printf("  -i, --interpret    Run in interpreter mode\n");
    printf("  -t, --target <arch> Target architecture (x86, x64, arm)\n");
    printf("  --stdlib <path>    Set standard library path\n");
    printf("  -h, --help         Show this help message\n\n");
    printf("Examples:\n");
    printf("  %s program.swf -o program\n", program_name);
    printf("  %s program.swf -i           # Run in interpreter\n", program_name);
    printf("  %s program.swf -emit-llvm   # Generate LLVM IR\n", program_name);
}

// Process command line arguments
void process_args(int argc, char** argv) {
    config = config_create_default();
    
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                config->output_file = str_copy(argv[++i]);
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            config->verbose = true;
            config->debug = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            config->debug = true;
        } else if (strcmp(argv[i], "-q") == 0 || strcmp(argv[i], "--quiet") == 0) {
            config->warnings = false;
        } else if (strncmp(argv[i], "-O", 2) == 0) {
            config->optimization_level = atoi(argv[i] + 2);
            config->optimize = config->optimization_level > 0;
        } else if (strcmp(argv[i], "-emit-llvm") == 0) {
            config->emit_llvm = true;
            config->output_format = "ll";
        } else if (strcmp(argv[i], "-emit-asm") == 0) {
            config->emit_asm = true;
            config->output_format = "asm";
        } else if (strcmp(argv[i], "-c") == 0) {
            config->link = false;
        } else if (strcmp(argv[i], "-I") == 0) {
            if (i + 1 < argc) {
                config_add_import_path(config, argv[++i]);
            }
        } else if (strcmp(argv[i], "-i") == 0 || strcmp(argv[i], "--interpret") == 0) {
            config->interpret = true;
        } else if (strcmp(argv[i], "-t") == 0 || strcmp(argv[i], "--target") == 0) {
            if (i + 1 < argc) {
                config->target_arch = str_copy(argv[++i]);
            }
        } else if (strcmp(argv[i], "--stdlib") == 0) {
            if (i + 1 < argc) {
                // This will be handled by config
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else if (argv[i][0] != '-') {
            // Input file
            config->input_file = str_copy(argv[i]);
        }
    }
    
    // Set default output file if not specified
    if (!config->output_file && config->input_file) {
        char* base = str_copy(config->input_file);
        char* dot = strrchr(base, '.');
        if (dot) *dot = '\0';
        config->output_file = str_format("%s.out", base);
        free(base);
    }
}

// Compile and execute
int compile_and_execute() {
    LOG(LOG_INFO, "SwiftFlow Compiler v%s", SWIFTFLOW_VERSION_STRING);
    
    if (!config->input_file) {
        LOG(LOG_ERROR, "No input file specified");
        return 1;
    }
    
    // Check file exists
    FILE* test = fopen(config->input_file, "r");
    if (!test) {
        LOG(LOG_ERROR, "Cannot open input file: %s", config->input_file);
        return 1;
    }
    fclose(test);
    
    // Check file extension
    if (!str_endswith(config->input_file, ".swf")) {
        LOG(LOG_WARNING, "File extension should be .swf");
    }
    
    // Read source file
    LOG(LOG_INFO, "Reading file: %s", config->input_file);
    char* source = read_file(config->input_file);
    if (!source) {
        LOG(LOG_ERROR, "Failed to read source file");
        return 1;
    }
    
    // Lexical analysis
    LOG(LOG_INFO, "Performing lexical analysis...");
    Lexer lexer;
    lexer_init(&lexer, source, config->input_file);
    
    // Parse tokens
    LOG(LOG_INFO, "Parsing...");
    Parser parser;
    parser_init(&parser, &lexer);
    
    ASTNode* ast = parse_program(&parser);
    if (!ast || parser.had_error) {
        LOG(LOG_ERROR, "Parsing failed");
        free(source);
        return 1;
    }
    
    LOG(LOG_INFO, "AST generated successfully");
    
    if (config->debug) {
        printf("\n%s=== AST Structure ===%s\n", COLOR_CYAN, COLOR_RESET);
        ast_print(ast, 0);
        printf("\n");
    }
    
    // Optimize AST if requested
    if (config->optimize) {
        LOG(LOG_INFO, "Optimizing AST (level %d)...", config->optimization_level);
        ASTNode* optimized = ast_optimize(ast);
        if (optimized != ast) {
            ast_free(ast);
            ast = optimized;
        }
    }
    
    // If in interpreter mode, run directly
    if (config->interpret) {
        LOG(LOG_INFO, "Running in interpreter mode...");
        // TODO: Implement interpreter
        printf("%s[INTERPRETER]%s Interpreter mode not yet implemented\n", 
               COLOR_YELLOW, COLOR_RESET);
    } else {
        // Choose backend based on flags
        BackendTarget target = BACKEND_LLVM;
        if (config->emit_asm) {
            target = BACKEND_NASM;
        } else if (config->interpret) {
            target = BACKEND_INTERPRETER;
        }
        
        // Compile
        LOG(LOG_INFO, "Compiling with %s backend...", 
            target == BACKEND_LLVM ? "LLVM" : 
            target == BACKEND_NASM ? "NASM" : "Interpreter");
        
        BackendContext* backend = backend_create(target, config->output_file);
        if (!backend) {
            LOG(LOG_ERROR, "Failed to create backend");
            ast_free(ast);
            free(source);
            return 1;
        }
        
        backend_compile(backend, ast);
        backend_free(backend);
        
        LOG(LOG_INFO, "Compilation completed: %s", config->output_file);
    }
    
    // Cleanup
    ast_free(ast);
    free(source);
    
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage(argv[0]);
        return 1;
    }
    
    process_args(argc, argv);
    
    int result = compile_and_execute();
    
    if (config) {
        config_free(config);
    }
    
    return result;
}

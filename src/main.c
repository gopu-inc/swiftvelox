#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "backend.h"

// Global configuration
SwiftFlowConfig config = {
    .verbose = false,
    .debug = false,
    .warnings = true,
    .optimize = true,
    .emit_llvm = false,
    .emit_asm = false,
    .link = true,
    .output_file = "a.out",
    .import_paths = NULL,
    .import_path_count = 0
};

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
    
    if (level == LOG_DEBUG && !config.debug) return;
    if (level == LOG_TRACE && !config.verbose) return;
    
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
    
    fread(buffer, 1, size, file);
    buffer[size] = '\0';
    
    fclose(file);
    return buffer;
}

// Process command line arguments
void process_args(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 < argc) {
                config.output_file = argv[++i];
            }
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            config.verbose = true;
            config.debug = true;
        } else if (strcmp(argv[i], "-d") == 0 || strcmp(argv[i], "--debug") == 0) {
            config.debug = true;
        } else if (strcmp(argv[i], "-W") == 0) {
            config.warnings = true;
        } else if (strcmp(argv[i], "-O0") == 0) {
            config.optimize = false;
        } else if (strcmp(argv[i], "-emit-llvm") == 0) {
            config.emit_llvm = true;
        } else if (strcmp(argv[i], "-emit-asm") == 0) {
            config.emit_asm = true;
        } else if (strcmp(argv[i], "-c") == 0) {
            config.link = false;
        } else if (strcmp(argv[i], "-I") == 0) {
            if (i + 1 < argc) {
                // Add import path
                config.import_path_count++;
                config.import_paths = realloc(config.import_paths, 
                    config.import_path_count * sizeof(char*));
                config.import_paths[config.import_path_count - 1] = argv[++i];
            }
        } else if (argv[i][0] != '-') {
            // Input file
            config.input_file = argv[i];
        }
    }
}

int main(int argc, char** argv) {
    printf("%sSwiftFlow Compiler/Interpreter v1.0%s\n", COLOR_CYAN, COLOR_RESET);
    printf("=====================================\n");
    
    if (argc < 2) {
        printf("Usage: %s <input.swf> [options]\n", argv[0]);
        printf("Options:\n");
        printf("  -o <file>     Output file name\n");
        printf("  -v, --verbose Verbose output\n");
        printf("  -d, --debug   Debug mode\n");
        printf("  -O0           Disable optimization\n");
        printf("  -emit-llvm    Emit LLVM IR\n");
        printf("  -emit-asm     Emit assembly\n");
        printf("  -c            Compile only, don't link\n");
        printf("  -I <path>     Add import search path\n");
        return 1;
    }
    
    process_args(argc, argv);
    
    if (!config.input_file) {
        LOG(LOG_ERROR, "No input file specified");
        return 1;
    }
    
    // Check file extension
    const char* ext = strrchr(config.input_file, '.');
    if (!ext || strcmp(ext, ".swf") != 0) {
        LOG(LOG_WARNING, "File extension should be .swf");
    }
    
    // Read source file
    LOG(LOG_INFO, "Reading file: %s", config.input_file);
    char* source = read_file(config.input_file);
    if (!source) {
        LOG(LOG_ERROR, "Failed to read source file");
        return 1;
    }
    
    // Lexical analysis
    LOG(LOG_INFO, "Performing lexical analysis...");
    Lexer lexer;
    lexer_init(&lexer, source, config.input_file);
    
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
    
    // Optimize AST if requested
    if (config.optimize) {
        LOG(LOG_INFO, "Optimizing AST...");
        ast = ast_optimize(ast);
    }
    
    // Choose backend based on flags
    BackendTarget target = BACKEND_INTERPRETER;
    if (config.emit_llvm) target = BACKEND_LLVM;
    else if (config.emit_asm) target = BACKEND_NASM;
    
    // Compile
    LOG(LOG_INFO, "Compiling...");
    BackendContext* backend = backend_create(target, config.output_file);
    if (!backend) {
        LOG(LOG_ERROR, "Failed to create backend");
        ast_free(ast);
        free(source);
        return 1;
    }
    
    backend_compile(backend, ast);
    backend_free(backend);
    
    LOG(LOG_INFO, "Compilation completed: %s", config.output_file);
    
    // Cleanup
    ast_free(ast);
    free(source);
    
    return 0;
}

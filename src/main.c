/*
[file name]: main.c
[file content begin]
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>
#include "common.h"
#include "lexer.h"
#include "parser.h"
#include "ast.h"
#include "interpreter.h"

// Global configuration
SwiftFlowConfig* config = NULL;

// HTTP server structure
typedef struct {
    int port;
    char* host;
    char* root_dir;
    bool dev_mode;
    int socket_fd;
} HttpServer;

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
    if (!filename) {
        LOG(LOG_ERROR, "Filename is NULL");
        return NULL;
    }
    
    FILE* file = fopen(filename, "rb");
    if (!file) {
        LOG(LOG_ERROR, "Cannot open file: %s", filename);
        return NULL;
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    
    if (size <= 0) {
        LOG(LOG_ERROR, "File is empty: %s", filename);
        fclose(file);
        return NULL;
    }
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        LOG(LOG_ERROR, "Memory allocation failed");
        fclose(file);
        return NULL;
    }
    
    size_t read_size = fread(buffer, 1, size, file);
    if (read_size != (size_t)size) {
        LOG(LOG_ERROR, "Failed to read entire file: %s", filename);
        free(buffer);
        fclose(file);
        return NULL;
    }
    
    buffer[read_size] = '\0';
    
    fclose(file);
    return buffer;
}

// Print usage information
void print_usage(const char* program_name) {
    printf("%sSwiftFlow Interpreter v%s%s\n", 
           COLOR_CYAN, SWIFTFLOW_VERSION_STRING, COLOR_RESET);
    printf("==============================================\n\n");
    printf("Usage: %s [options] <file.swf>|<command>\n\n", program_name);
    printf("Commands:\n");
    printf("  run <file>        Run SwiftFlow program\n");
    printf("  repl              Start interactive REPL\n");
    printf("  compile <file>    Compile to bytecode\n");
    printf("\nOptions:\n");
    printf("  -v, --verbose        Verbose output\n");
    printf("  -d, --debug          Debug mode\n");
    printf("  -q, --quiet          Quiet mode (no warnings)\n");
    printf("  -r, --run <code>     Run code directly\n");
    printf("  -c, --compile        Compile mode\n");
    printf("  -s, --server         Start HTTP server\n");
    printf("  -o, --optimize       Enable optimizations\n");
    printf("  -I <path>            Add import search path\n");
    printf("  --port <port>        HTTP server port (default: 8080)\n");
    printf("  --host <host>        HTTP server host (default: 0.0.0.0)\n");
    printf("  --dev                Development mode\n");
    printf("  --stdin              Read from stdin\n");
    printf("  --help               Show this help\n");
    printf("\nExamples:\n");
    printf("  %s program.swf\n", program_name);
    printf("  %s -r \"print('Hello World')\"\n", program_name);
    printf("  %s -cs program.swf --optimize\n", program_name);
    printf("  %s run --serv:8080:host:0.0.0.0 --dev\n", program_name);
    printf("  %s repl\n", program_name);
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
        } else if (strcmp(argv[i], "-o") == 0 || strcmp(argv[i], "--optimize") == 0) {
            config->optimize = true;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compile") == 0) {
            config->interpret = false;
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) {
            // Server mode
            config->input_file = str_copy("server");
            
        // process_args
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--run") == 0) {
            if (i + 1 < argc) {
                config->input_file = str_copy("-inline-");
                // On saute l'argument suivant pour qu'il ne soit pas pris comme un fichier
                i++; 
            }
        } else if (strcmp(argv[i], "--stdin") == 0) {
            config->input_file = str_copy("-");
        } else if (strcmp(argv[i], "--dev") == 0) {
            config->debug = true;
            config->verbose = true;
        } else if (strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                // Store port for server mode
                i++;
            }
        } else if (strcmp(argv[i], "--host") == 0) {
            if (i + 1 < argc) {
                i++;
            }
        } else if (strcmp(argv[i], "-I") == 0) {
            if (i + 1 < argc) {
                config_add_import_path(config, argv[++i]);
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            exit(EXIT_SUCCESS);
        } else if (strcmp(argv[i], "repl") == 0) {
            config->input_file = str_copy("-repl-");
        } else if (strcmp(argv[i], "run") == 0) {
            if (i + 1 < argc) {
                config->input_file = str_copy(argv[++i]);
            }
        } else if (argv[i][0] != '-') {
            // Input file
            if (config->input_file && strcmp(config->input_file, "-inline-") != 0) {
                LOG(LOG_WARNING, "Multiple input files specified, using: %s", config->input_file);
            } else {
                config->input_file = str_copy(argv[i]);
            }
        }
    }
    
    // Default to REPL if no input
    if (!config->input_file && argc == 1) {
        config->input_file = str_copy("-repl-");
    }
}

// Read from stdin
char* read_stdin() {
    printf("%sReading from stdin (Ctrl+D to finish)...%s\n", COLOR_YELLOW, COLOR_RESET);
    
    size_t buffer_size = 4096;
    char* buffer = malloc(buffer_size);
    if (!buffer) return NULL;
    
    size_t total_read = 0;
    size_t read;
    
    while ((read = fread(buffer + total_read, 1, buffer_size - total_read - 1, stdin)) > 0) {
        total_read += read;
        if (total_read >= buffer_size - 1) {
            buffer_size *= 2;
            char* new_buffer = realloc(buffer, buffer_size);
            if (!new_buffer) {
                free(buffer);
                return NULL;
            }
            buffer = new_buffer;
        }
    }
    
    if (total_read == 0) {
        free(buffer);
        return NULL;
    }
    
    buffer[total_read] = '\0';
    return buffer;
}

// Simple HTTP server function
void start_http_server(int port, const char* host, bool dev_mode) {
    LOG(LOG_INFO, "Starting HTTP server on %s:%d", host, port);
    
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    
    // Create socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        LOG(LOG_ERROR, "Socket creation failed");
        return;
    }
    
    // Set socket options
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        LOG(LOG_ERROR, "Socket options failed");
        close(server_fd);
        return;
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host);
    address.sin_port = htons(port);
    
    // Bind socket
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) {
        LOG(LOG_ERROR, "Bind failed");
        close(server_fd);
        return;
    }
    
    // Listen
    if (listen(server_fd, 10) < 0) {
        LOG(LOG_ERROR, "Listen failed");
        close(server_fd);
        return;
    }
    
    LOG(LOG_INFO, "HTTP server started. Press Ctrl+C to stop.");
    
    // Signal handling for graceful shutdown
    signal(SIGINT, SIG_DFL);
    
    // Main server loop
    while (1) {
        int client_fd;
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);
        
        client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
        if (client_fd < 0) {
            LOG(LOG_ERROR, "Accept failed");
            continue;
        }
        
        // Read request
        char buffer[4096] = {0};
        read(client_fd, buffer, 4095);
        
        // Parse request
        char method[16], path[1024];
        sscanf(buffer, "%s %s", method, path);
        
        LOG(LOG_INFO, "Request: %s %s", method, path);
        
        // Serve file
        char file_path[2048];
        if (strcmp(path, "/") == 0) {
            strcpy(file_path, "./index.html");
        } else {
            snprintf(file_path, sizeof(file_path), ".%s", path);
        }
        
        // Check if file exists
        FILE* file = fopen(file_path, "rb");
        if (file) {
            fseek(file, 0, SEEK_END);
            long file_size = ftell(file);
            rewind(file);
            
            char* file_content = malloc(file_size + 1);
            fread(file_content, 1, file_size, file);
            fclose(file);
            
            // Determine content type
            const char* content_type = "text/plain";
            if (str_endswith(file_path, ".html")) content_type = "text/html";
            else if (str_endswith(file_path, ".css")) content_type = "text/css";
            else if (str_endswith(file_path, ".js")) content_type = "application/javascript";
            else if (str_endswith(file_path, ".json")) content_type = "application/json";
            else if (str_endswith(file_path, ".png")) content_type = "image/png";
            else if (str_endswith(file_path, ".jpg") || str_endswith(file_path, ".jpeg")) 
                content_type = "image/jpeg";
            
            // Send response
            char header[1024];
            int header_len = snprintf(header, sizeof(header),
                "HTTP/1.1 200 OK\r\n"
                "Content-Type: %s\r\n"
                "Content-Length: %ld\r\n"
                "Connection: close\r\n"
                "\r\n", content_type, file_size);
            
            write(client_fd, header, header_len);
            write(client_fd, file_content, file_size);
            
            free(file_content);
        } else {
            // 404 Not Found
            char* response = "HTTP/1.1 404 Not Found\r\n"
                            "Content-Type: text/html\r\n"
                            "Content-Length: 48\r\n"
                            "\r\n"
                            "<html><body><h1>404 Not Found</h1></body></html>";
            write(client_fd, response, strlen(response));
        }
        
        close(client_fd);
    }
    
    close(server_fd);
}

// Execute SwiftFlow code from source
int execute_swiftflow(const char* source, const char* filename, bool compile_only) {
    if (!source || !source[0]) {
        LOG(LOG_ERROR, "Empty source code");
        return 1;
    }
    
    if (config->verbose) {
        LOG(LOG_INFO, "SwiftFlow Interpreter v%s", SWIFTFLOW_VERSION_STRING);
        LOG(LOG_INFO, "Executing: %s", filename);
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
    
    if (compile_only) {
        // Compilation mode
        LOG(LOG_INFO, "Compilation complete");
        ast_free(ast);
        free(lexer.filename);
        return 0;
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
    
    SwiftFlowInterpreter* interpreter = interpreter_new();
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
            printf("  vars        - Show defined variables\n");
            printf("  ast <expr>  - Show AST for expression\n");
            printf("  tokens <code> - Show tokens for code\n");
            continue;
        }
        
        if (strcmp(line, "clear") == 0) {
            printf("\033[2J\033[H"); // Clear screen
            continue;
        }
        
        if (strcmp(line, "vars") == 0) {
            interpreter_dump_environment(interpreter);
            continue;
        }
        
        if (strncmp(line, "ast ", 4) == 0) {
            // Parse and show AST
            const char* code = line + 4;
            Lexer lexer;
            lexer_init(&lexer, code, "<repl>");
            Parser parser;
            parser_init(&parser, &lexer);
            ASTNode* ast = parse_program(&parser);
            if (ast) {
                ast_print(ast, 0);
                ast_free(ast);
            }
            continue;
        }
        
        if (strncmp(line, "tokens ", 7) == 0) {
            // Show tokens
            const char* code = line + 7;
            Lexer lexer;
            lexer_init(&lexer, code, "<repl>");
            Token token;
            do {
                token = lexer_next_token(&lexer);
                printf("Token: %s (%.*s)\n", 
                       token_kind_to_string(token.kind),
                       token.length, token.start);
            } while (token.kind != TK_EOF);
            continue;
        }
        
        // Execute the line
        int result = execute_swiftflow(line, "<repl>", false);
        
        if (result != 0) {
            printf("%sExecution failed%s\n", COLOR_RED, COLOR_RESET);
        }
        
        line_num++;
    }
    
    interpreter_free(interpreter);
    printf("\nGoodbye!\n");
}

int main(int argc, char** argv) {
    process_args(argc, argv);
    
    // Handle server mode
    if (config->input_file && strcmp(config->input_file, "server") == 0) {
        int port = 8080;
        const char* host = "0.0.0.0";
        bool dev_mode = config->debug;
        
        // Parse additional server arguments
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--serv:", 7) == 0) {
                // Format: --serv:8080:host:0.0.0.0
                char* token = strtok(argv[i] + 7, ":");
                if (token) port = atoi(token);
                token = strtok(NULL, ":");
                if (token && strcmp(token, "host") == 0) {
                    token = strtok(NULL, ":");
                    if (token) host = token;
                }
            }
        }
        
        start_http_server(port, host, dev_mode);
        config_free(config);
        return 0;
    }
    
    char* source = NULL;
    char* filename = config->input_file;
    int result = 0;
    
    if (filename && strcmp(filename, "-repl-") == 0) {
        // REPL mode
        run_repl();
        config_free(config);
        return 0;
    }
    
    if (filename && strcmp(filename, "-inline-") == 0) {
        // Direct code execution
        for (int i = 1; i < argc; i++) {
            if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--run") == 0) && i + 1 < argc) {
                source = str_copy(argv[i + 1]);
                filename = "<command-line>";
                break;
            }
        }
    } else if (filename && strcmp(filename, "-") == 0) {
        // Read from stdin
        source = read_stdin();
        filename = "<stdin>";
    } else if (filename) {
        // Read from file
        if (filename && !str_endswith(filename, ".swf")) {
            LOG(LOG_WARNING, "File '%s' doesn't have .swf extension", filename);
        }
        
        source = read_file(filename);
    }
    
    if (!source) {
        LOG(LOG_ERROR, "No source code to execute");
        config_free(config);
        return 1;
    }
    
    // Check if compile only
    bool compile_only = false;
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compile") == 0) {
            compile_only = true;
            break;
        }
    }
    
    result = execute_swiftflow(source, filename, compile_only);
    
    free(source);
    config_free(config);
    return result;
}
/*
    [file content end]
*/

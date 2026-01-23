/*
[file name]: src/main.c
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

// Logging implementation
void swiftflow_log(LogLevel level, const char* file, int line, const char* fmt, ...) {
    const char* level_str[] = { "FATAL", "ERROR", "WARNING", "INFO", "DEBUG", "TRACE" };
    const char* level_color[] = { COLOR_BRIGHT_RED, COLOR_RED, COLOR_YELLOW, COLOR_GREEN, COLOR_CYAN, COLOR_BRIGHT_BLACK };
    
    if (!config) { if (level == LOG_DEBUG || level == LOG_TRACE) return; }
    else {
        if (level == LOG_DEBUG && !config->debug) return;
        if (level == LOG_TRACE && !config->verbose) return;
        if (level == LOG_WARNING && !config->warnings) return;
    }
    
    fprintf(stderr, "%s[%s]%s %s:%d: ", level_color[level], level_str[level], COLOR_RESET, file, line);
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
    fprintf(stderr, "\n");
    if (level == LOG_FATAL) exit(EXIT_FAILURE);
}

char* read_file(const char* filename) {
    if (!filename) return NULL;
    FILE* file = fopen(filename, "rb");
    if (!file) { LOG(LOG_ERROR, "Cannot open file: %s", filename); return NULL; }
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    if (size <= 0) { fclose(file); return NULL; }
    char* buffer = malloc(size + 1);
    size_t read_size = fread(buffer, 1, size, file);
    buffer[read_size] = '\0';
    fclose(file);
    return buffer;
}

char* read_stdin() {
    printf("%sReading from stdin...%s\n", COLOR_YELLOW, COLOR_RESET);
    size_t buffer_size = 4096;
    char* buffer = malloc(buffer_size);
    size_t total_read = 0;
    size_t read;
    while ((read = fread(buffer + total_read, 1, buffer_size - total_read - 1, stdin)) > 0) {
        total_read += read;
        if (total_read >= buffer_size - 1) {
            buffer_size *= 2;
            buffer = realloc(buffer, buffer_size);
        }
    }
    buffer[total_read] = '\0';
    return buffer;
}

void start_http_server(int port, const char* host, bool dev_mode) {
    (void)dev_mode; // Fix unused parameter warning
    LOG(LOG_INFO, "Starting HTTP server on %s:%d", host, port);
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) return;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) return;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr(host);
    address.sin_port = htons(port);
    if (bind(server_fd, (struct sockaddr*)&address, sizeof(address)) < 0) return;
    if (listen(server_fd, 10) < 0) return;
    LOG(LOG_INFO, "Server running...");
    while(1) {
        int client_fd = accept(server_fd, NULL, NULL);
        if (client_fd >= 0) {
            write(client_fd, "HTTP/1.1 200 OK\r\n\r\nHello SwiftFlow!", 35);
            close(client_fd);
        }
    }
}

int execute_swiftflow(const char* source, const char* filename, bool compile_only) {
    if (!source || !source[0]) return 1;
    Lexer lexer; lexer_init(&lexer, source, filename);
    Parser parser; parser_init(&parser, &lexer);
    ASTNode* ast = parse_program(&parser);
    
    if (parser.had_error || !ast) {
        if(ast) ast_free(ast);
        free(lexer.filename);
        return 1;
    }
    
    // Si compile_only est vrai, on s'arrête ici
    if (compile_only) {
        LOG(LOG_INFO, "Compilation complete");
        ast_free(ast);
        free(lexer.filename);
        return 0;
    }
    
    // Sinon on exécute
    SwiftFlowInterpreter* interpreter = interpreter_new();
    interpreter->debug_mode = config->debug;
    int result = interpreter_run(interpreter, ast);
    interpreter_free(interpreter);
    ast_free(ast);
    free(lexer.filename);
    return result;
}

// Process args handles: flags, filename detection
void process_args(int argc, char** argv) {
    config = config_create_default();
    // Par défaut, on interprète le code (pas de compilation seule)
    config->interpret = true; 

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--verbose") == 0) {
            config->verbose = true;
        } else if (strcmp(argv[i], "-c") == 0 || strcmp(argv[i], "--compile") == 0) {
            config->interpret = false; // Mode compilation seule activé
        } else if (strcmp(argv[i], "-s") == 0 || strcmp(argv[i], "--server") == 0) {
            config->input_file = str_copy("server");
        } else if (strncmp(argv[i], "--serv:", 7) == 0) {
            config->input_file = str_copy("server");
        } else if (strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--run") == 0) {
            if (i + 1 < argc) {
                config->input_file = str_copy("-inline-");
                i++; // Skip code argument here so it's not treated as file
            }
        } else if (strcmp(argv[i], "run") == 0) {
            // Ignore 'run' keyword if next arg is a flag or empty
            if (i + 1 < argc && argv[i+1][0] != '-') {
                config->input_file = str_copy(argv[++i]);
            }
        } else if (argv[i][0] != '-') {
            if (!config->input_file) config->input_file = str_copy(argv[i]);
        }
    }
}

int main(int argc, char** argv) {
    process_args(argc, argv);
    
    // Server mode
    if (config->input_file && strcmp(config->input_file, "server") == 0) {
        int port = 8080;
        const char* host = "0.0.0.0";
        for (int i = 1; i < argc; i++) {
            if (strncmp(argv[i], "--serv:", 7) == 0) {
                char* t = strtok(argv[i] + 7, ":");
                if (t) port = atoi(t);
            } else if (strcmp(argv[i], "--port") == 0 && i+1 < argc) port = atoi(argv[++i]);
            else if (strcmp(argv[i], "--host") == 0 && i+1 < argc) host = argv[++i];
        }
        start_http_server(port, host, config->debug);
        return 0;
    }
    
    char* source = NULL;
    char* filename = config->input_file;
    
    if (filename && strcmp(filename, "-inline-") == 0) {
        for (int i = 1; i < argc; i++) {
            if ((strcmp(argv[i], "-r") == 0 || strcmp(argv[i], "--run") == 0) && i + 1 < argc) {
                source = str_copy(argv[i + 1]);
                filename = "<inline>";
                break;
            }
        }
    } else if (filename && strcmp(filename, "-") == 0) {
        source = read_stdin();
        filename = "<stdin>";
    } else if (filename) {
        source = read_file(filename);
    } else {
        // Mode REPL par défaut si pas de fichier
        filename = "<repl>";
        run_repl();
        config_free(config);
        return 0;
    }
    
    if (!source && strcmp(filename, "<repl>") != 0) {
        LOG(LOG_ERROR, "No source code to execute");
        config_free(config);
        return 1;
    }
    
    // Détermine le mode d'exécution
    // Si config->interpret est true (défaut), compile_only est false -> on exécute.
    // Si -c est passé, config->interpret est false -> compile_only est true -> on compile juste.
    bool compile_only = !config->interpret; 
    
    execute_swiftflow(source, filename, compile_only);
    
    if(source) free(source);
    config_free(config);
    return 0;
}

// io.c - Module IO simplifié pour SwiftFlow
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include "common.h"

// ======================================================
// [SECTION] GESTION DES DESCRIPTEURS DE FICHIER
// ======================================================
typedef struct {
    int id;
    char* name;
    FILE* handle;
    char* mode;
    bool is_open;
    int position;
    int size;
} FileDescriptor;

static FileDescriptor file_descriptors[256];
static int fd_count = 0;
static int next_fd = 3; // 0,1,2 sont stdin, stdout, stderr

// ======================================================
// [SECTION] FONCTIONS UTILITAIRES
// ======================================================
static int allocate_fd() {
    if (fd_count >= 256) return -1;
    
    for (int i = next_fd; i < 256; i++) {
        if (!file_descriptors[i].is_open) {
            file_descriptors[i].id = i;
            file_descriptors[i].is_open = true;
            fd_count++;
            next_fd = i + 1;
            return i;
        }
    }
    return -1;
}

static FileDescriptor* get_fd(int fd) {
    if (fd < 0 || fd >= 256 || !file_descriptors[fd].is_open) {
        return NULL;
    }
    return &file_descriptors[fd];
}

static void close_fd(int fd) {
    FileDescriptor* desc = get_fd(fd);
    if (desc) {
        if (desc->handle) fclose(desc->handle);
        if (desc->name) free(desc->name);
        if (desc->mode) free(desc->mode);
        
        memset(desc, 0, sizeof(FileDescriptor));
        desc->id = fd;
        fd_count--;
    }
}

// ======================================================
// [SECTION] FONCTIONS D'EXÉCUTION IO SIMPLIFIÉES
// ======================================================
void io_open(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Opening file...\n", COLOR_CYAN, COLOR_RESET);
    
    // Note: Pour l'instant, on va juste afficher un message
    // L'intégration complète viendra plus tard
    printf("%s[IO INFO]%s File open operation (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_close(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Closing file...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s File close operation (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_read(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Reading file...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s File read operation (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_write(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Writing to file...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s File write operation (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_seek(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Seeking in file...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s File seek operation (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_tell(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Getting file position...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s File tell operation (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_exists(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Checking if file exists...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s File exists check (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_isfile(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Checking if path is a file...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s Is file check (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_isdir(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Checking if path is a directory...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s Is directory check (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_mkdir(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Creating directory...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s Directory creation (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

void io_listdir(ASTNode* node) {
    if (!node) return;
    
    printf("%s[IO]%s Listing directory contents...\n", COLOR_CYAN, COLOR_RESET);
    printf("%s[IO INFO]%s Directory listing (to be implemented)\n", 
           COLOR_YELLOW, COLOR_RESET);
}

// ======================================================
// [SECTION] FONCTION D'INITIALISATION
// ======================================================
void init_io_module() {
    printf("%s[IO MODULE]%s Initializing...\n", COLOR_CYAN, COLOR_RESET);
    
    // Initialiser stdin, stdout, stderr
    file_descriptors[0].id = 0;
    file_descriptors[0].name = str_copy("stdin");
    file_descriptors[0].handle = stdin;
    file_descriptors[0].mode = str_copy("r");
    file_descriptors[0].is_open = true;
    file_descriptors[0].position = 0;
    
    file_descriptors[1].id = 1;
    file_descriptors[1].name = str_copy("stdout");
    file_descriptors[1].handle = stdout;
    file_descriptors[1].mode = str_copy("w");
    file_descriptors[1].is_open = true;
    file_descriptors[1].position = 0;
    
    file_descriptors[2].id = 2;
    file_descriptors[2].name = str_copy("stderr");
    file_descriptors[2].handle = stderr;
    file_descriptors[2].mode = str_copy("w");
    file_descriptors[2].is_open = true;
    file_descriptors[2].position = 0;
    
    fd_count = 3;
    
    printf("%s[IO MODULE]%s Initialized with %d file descriptors\n", 
           COLOR_GREEN, COLOR_RESET, fd_count);
    printf("%s[IO MODULE]%s Basic IO operations available\n", 
           COLOR_GREEN, COLOR_RESET);
}

// io.c - Module IO autonome pour SwiftFlow
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>
#include <time.h>
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
    long position;
    long size;
} FileDescriptor;

static FileDescriptor file_descriptors[256];
static int fd_count = 0;

// ======================================================
// [SECTION] FONCTIONS UTILITAIRES
// ======================================================
static int allocate_fd() {
    for (int i = 3; i < 256; i++) {
        if (!file_descriptors[i].is_open) {
            file_descriptors[i].id = i;
            file_descriptors[i].is_open = true;
            fd_count++;
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

// Fonctions simplifiées pour extraire les valeurs des nœuds AST
static char* extract_string(ASTNode* node) {
    if (!node) return NULL;
    
    if (node->type == NODE_STRING) {
        return str_copy(node->data.str_val);
    }
    
    if (node->type == NODE_IDENT && node->data.name) {
        return str_copy(node->data.name);
    }
    
    // Pour les autres types, retourner une représentation par défaut
    char* result = malloc(32);
    if (result) {
        if (node->type == NODE_INT) {
            snprintf(result, 32, "%lld", node->data.int_val);
        } else if (node->type == NODE_FLOAT) {
            snprintf(result, 32, "%g", node->data.float_val);
        } else if (node->type == NODE_BOOL) {
            strcpy(result, node->data.bool_val ? "true" : "false");
        } else {
            strcpy(result, "");
        }
    }
    return result;
}

static double extract_number(ASTNode* node) {
    if (!node) return 0.0;
    
    if (node->type == NODE_INT) {
        return (double)node->data.int_val;
    }
    
    if (node->type == NODE_FLOAT) {
        return node->data.float_val;
    }
    
    if (node->type == NODE_BOOL) {
        return node->data.bool_val ? 1.0 : 0.0;
    }
    
    return 0.0;
}

// ======================================================
// [SECTION] FONCTIONS D'EXÉCUTION IO
// ======================================================
void io_open(ASTNode* node) {
    if (!node) return;
    
    char* filename = extract_string(node->left);
    char* mode = extract_string(node->right);
    char* var_name = node->third ? extract_string(node->third) : NULL;
    
    if (!filename || !mode) {
        printf("%s[IO ERROR]%s Missing filename or mode\n", COLOR_RED, COLOR_RESET);
        if (filename) free(filename);
        if (mode) free(mode);
        if (var_name) free(var_name);
        return;
    }
    
    // Valider le mode
    const char* valid_modes[] = {"r", "w", "a", "r+", "w+", "a+", 
                                "rb", "wb", "ab", "r+b", "w+b", "a+b", NULL};
    bool valid_mode = false;
    
    for (int i = 0; valid_modes[i]; i++) {
        if (strcmp(mode, valid_modes[i]) == 0) {
            valid_mode = true;
            break;
        }
    }
    
    if (!valid_mode) {
        printf("%s[IO ERROR]%s Invalid file mode: %s\n", COLOR_RED, COLOR_RESET, mode);
        free(filename);
        free(mode);
        if (var_name) free(var_name);
        return;
    }
    
    // Ouvrir le fichier
    FILE* f = fopen(filename, mode);
    if (!f) {
        printf("%s[IO ERROR]%s Cannot open file: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, filename, strerror(errno));
        free(filename);
        free(mode);
        if (var_name) free(var_name);
        return;
    }
    
    int fd = allocate_fd();
    if (fd == -1) {
        printf("%s[IO ERROR]%s Too many open files\n", COLOR_RED, COLOR_RESET);
        fclose(f);
        free(filename);
        free(mode);
        if (var_name) free(var_name);
        return;
    }
    
    FileDescriptor* desc = &file_descriptors[fd];
    desc->name = str_copy(filename);
    desc->handle = f;
    desc->mode = str_copy(mode);
    desc->is_open = true;
    
    // Obtenir la taille
    fseek(f, 0, SEEK_END);
    desc->size = ftell(f);
    fseek(f, 0, SEEK_SET);
    desc->position = 0;
    
    printf("%s[IO]%s File opened: %s (fd=%d, mode=%s, size=%ld)\n", 
           COLOR_GREEN, COLOR_RESET, filename, fd, mode, desc->size);
    
    if (var_name) {
        printf("%s[IO INFO]%s File descriptor %d would be stored in variable '%s'\n",
               COLOR_CYAN, COLOR_RESET, fd, var_name);
    }
    
    free(filename);
    free(mode);
    if (var_name) free(var_name);
}

void io_close(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing file descriptor\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = extract_number(node->left);
    int fd = (int)fd_val;
    
    FileDescriptor* desc = get_fd(fd);
    if (!desc) {
        printf("%s[IO ERROR]%s Invalid file descriptor: %d\n", COLOR_RED, COLOR_RESET, fd);
        return;
    }
    
    if (desc->handle) {
        fclose(desc->handle);
        desc->handle = NULL;
    }
    
    printf("%s[IO]%s File closed: %s (fd=%d)\n", 
           COLOR_GREEN, COLOR_RESET, desc->name ? desc->name : "unknown", fd);
    
    close_fd(fd);
}

void io_read(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing file descriptor\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = extract_number(node->left);
    int fd = (int)fd_val;
    
    FileDescriptor* desc = get_fd(fd);
    if (!desc) {
        printf("%s[IO ERROR]%s Invalid file descriptor: %d\n", COLOR_RED, COLOR_RESET, fd);
        return;
    }
    
    if (!desc->handle) {
        printf("%s[IO ERROR]%s File not open: fd=%d\n", COLOR_RED, COLOR_RESET, fd);
        return;
    }
    
    // Vérifier les permissions
    if (strchr(desc->mode, 'r') == NULL && strchr(desc->mode, '+') == NULL) {
        printf("%s[IO ERROR]%s File not opened for reading: %s\n", 
               COLOR_RED, COLOR_RESET, desc->mode);
        return;
    }
    
    size_t size = 1024;
    if (node->right) {
        size_t temp_size = (size_t)extract_number(node->right);
        if (temp_size > 0 && temp_size <= 65536) {
            size = temp_size;
        }
    }
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        printf("%s[IO ERROR]%s Memory allocation failed\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    size_t bytes_read = fread(buffer, 1, size, desc->handle);
    buffer[bytes_read] = '\0';
    
    desc->position = ftell(desc->handle);
    
    // Variable pour résultat
    char* var_name = node->third ? extract_string(node->third) : NULL;
    
    if (var_name) {
        printf("%s[IO]%s Read %zu bytes from fd=%d (would store in '%s')\n", 
               COLOR_GREEN, COLOR_RESET, bytes_read, fd, var_name);
        free(var_name);
    } else {
        printf("%s[IO]%s Read %zu bytes from fd=%d:\n", 
               COLOR_GREEN, COLOR_RESET, bytes_read, fd);
        printf("--- BEGIN CONTENT ---\n");
        printf("%s", buffer);
        printf("\n--- END CONTENT ---\n");
    }
    
    free(buffer);
}

void io_write(ASTNode* node) {
    if (!node || !node->left || !node->right) {
        printf("%s[IO ERROR]%s Missing file descriptor or data\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = extract_number(node->left);
    int fd = (int)fd_val;
    
    FileDescriptor* desc = get_fd(fd);
    if (!desc) {
        printf("%s[IO ERROR]%s Invalid file descriptor: %d\n", COLOR_RED, COLOR_RESET, fd);
        return;
    }
    
    if (!desc->handle) {
        printf("%s[IO ERROR]%s File not open: fd=%d\n", COLOR_RED, COLOR_RESET, fd);
        return;
    }
    
    // Vérifier les permissions
    if (strchr(desc->mode, 'w') == NULL && strchr(desc->mode, 'a') == NULL && 
        strchr(desc->mode, '+') == NULL) {
        printf("%s[IO ERROR]%s File not opened for writing: %s\n", 
               COLOR_RED, COLOR_RESET, desc->mode);
        return;
    }
    
    char* data = extract_string(node->right);
    if (!data) {
        return;
    }
    
    size_t bytes_written = fwrite(data, 1, strlen(data), desc->handle);
    desc->position = ftell(desc->handle);
    
    if (bytes_written != strlen(data)) {
        printf("%s[IO WARNING]%s Partial write: %zu/%zu bytes\n", 
               COLOR_YELLOW, COLOR_RESET, bytes_written, strlen(data));
    } else {
        printf("%s[IO]%s Wrote %zu bytes to fd=%d\n", 
               COLOR_GREEN, COLOR_RESET, bytes_written, fd);
    }
    
    free(data);
}

void io_seek(ASTNode* node) {
    if (!node || !node->left || !node->right) {
        printf("%s[IO ERROR]%s Missing file descriptor or position\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = extract_number(node->left);
    int fd = (int)fd_val;
    
    FileDescriptor* desc = get_fd(fd);
    if (!desc) {
        printf("%s[IO ERROR]%s Invalid file descriptor: %d\n", COLOR_RED, COLOR_RESET, fd);
        return;
    }
    
    if (!desc->handle) {
        printf("%s[IO ERROR]%s File not open: fd=%d\n", COLOR_RED, COLOR_RESET, fd);
        return;
    }
    
    int whence = SEEK_SET;
    if (node->third) {
        char* whence_str = extract_string(node->third);
        if (whence_str) {
            if (strcmp(whence_str, "cur") == 0 || strcmp(whence_str, "current") == 0) {
                whence = SEEK_CUR;
            } else if (strcmp(whence_str, "end") == 0) {
                whence = SEEK_END;
            } else if (strcmp(whence_str, "set") == 0 || strcmp(whence_str, "begin") == 0) {
                whence = SEEK_SET;
            }
            free(whence_str);
        }
    }
    
    long offset = (long)extract_number(node->right);
    
    if (fseek(desc->handle, offset, whence) != 0) {
        printf("%s[IO ERROR]%s Seek failed: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
        return;
    }
    
    desc->position = ftell(desc->handle);
    
    printf("%s[IO]%s Seek to position %ld (whence=%d) on fd=%d\n", 
           COLOR_GREEN, COLOR_RESET, offset, whence, fd);
}

void io_tell(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing file descriptor\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = extract_number(node->left);
    int fd = (int)fd_val;
    
    FileDescriptor* desc = get_fd(fd);
    if (!desc) {
        printf("%s[IO ERROR]%s Invalid file descriptor: %d\n", COLOR_RED, COLOR_RESET, fd);
        return;
    }
    
    if (!desc->handle) {
        printf("%s[IO ERROR]%s File not open: fd=%d\n", COLOR_RED, COLOR_RESET, fd);
        return;
    }
    
    long pos = ftell(desc->handle);
    desc->position = pos;
    
    // Variable pour résultat
    char* var_name = node->right ? extract_string(node->right) : NULL;
    
    if (var_name) {
        printf("%s[IO]%s Current position of fd=%d is %ld (would store in '%s')\n",
               COLOR_GREEN, COLOR_RESET, fd, pos, var_name);
        free(var_name);
    } else {
        printf("Position: %ld\n", pos);
    }
}

void io_exists(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing path\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* path = extract_string(node->left);
    if (!path) return;
    
    bool exists = (access(path, F_OK) == 0);
    
    // Variable pour résultat
    char* var_name = node->right ? extract_string(node->right) : NULL;
    
    if (var_name) {
        printf("%s[IO]%s Path '%s' exists: %s (would store in '%s')\n",
               COLOR_GREEN, COLOR_RESET, path, exists ? "yes" : "no", var_name);
        free(var_name);
    } else {
        printf("%s exists: %s\n", path, exists ? "yes" : "no");
    }
    
    free(path);
}

void io_isfile(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing path\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* path = extract_string(node->left);
    if (!path) return;
    
    struct stat st;
    bool is_file = (stat(path, &st) == 0 && S_ISREG(st.st_mode));
    
    // Variable pour résultat
    char* var_name = node->right ? extract_string(node->right) : NULL;
    
    if (var_name) {
        printf("%s[IO]%s Path '%s' is a file: %s\n",
               COLOR_GREEN, COLOR_RESET, path, is_file ? "yes" : "no");
        free(var_name);
    }
    
    free(path);
}

void io_isdir(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing path\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* path = extract_string(node->left);
    if (!path) return;
    
    struct stat st;
    bool is_dir = (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    
    // Variable pour résultat
    char* var_name = node->right ? extract_string(node->right) : NULL;
    
    if (var_name) {
        printf("%s[IO]%s Path '%s' is a directory: %s\n",
               COLOR_GREEN, COLOR_RESET, path, is_dir ? "yes" : "no");
        free(var_name);
    }
    
    free(path);
}

void io_mkdir(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing directory name\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* dirname_str = extract_string(node->left);
    if (!dirname_str) return;
    
    int mode = 0755;
    if (node->right) {
        mode = (int)extract_number(node->right);
    }
    
    if (mkdir(dirname_str, mode) != 0) {
        printf("%s[IO ERROR]%s Cannot create directory: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, dirname_str, strerror(errno));
    } else {
        printf("%s[IO]%s Directory created: %s\n", COLOR_GREEN, COLOR_RESET, dirname_str);
    }
    
    free(dirname_str);
}

void io_listdir(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing directory path\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* path = extract_string(node->left);
    if (!path) return;
    
    DIR* dir = opendir(path);
    if (!dir) {
        printf("%s[IO ERROR]%s Cannot open directory: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, path, strerror(errno));
        free(path);
        return;
    }
    
    printf("%s[IO]%s Contents of %s:\n", COLOR_GREEN, COLOR_RESET, path);
    printf("========================\n");
    
    struct dirent* entry;
    int count = 0;
    int dir_count = 0;
    int file_count = 0;
    long long total_size = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        
        struct stat st;
        const char* type = "?";
        char size_str[32] = "";
        char time_str[64] = "";
        
        if (stat(fullpath, &st) == 0) {
            if (S_ISREG(st.st_mode)) {
                type = "F";
                file_count++;
                total_size += st.st_size;
                
                if (st.st_size < 1024)
                    snprintf(size_str, sizeof(size_str), "%lld B", (long long)st.st_size);
                else if (st.st_size < 1024*1024)
                    snprintf(size_str, sizeof(size_str), "%.1f KB", st.st_size/1024.0);
                else
                    snprintf(size_str, sizeof(size_str), "%.1f MB", st.st_size/(1024.0*1024.0));
            }
            else if (S_ISDIR(st.st_mode)) {
                type = "D";
                dir_count++;
            }
            else if (S_ISLNK(st.st_mode)) type = "L";
            
            struct tm* tm_info = localtime(&st.st_mtime);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
        }
        
        if (S_ISDIR(st.st_mode)) {
            printf("  \033[1;34m[%s]\033[0m \033[1;34m%s/\033[0m %s\n", 
                   type, entry->d_name, time_str);
        } else if (S_ISREG(st.st_mode)) {
            printf("  [%s] %-30s %10s  %s\n", 
                   type, entry->d_name, size_str, time_str);
        } else {
            printf("  [%s] %s\n", type, entry->d_name);
        }
        
        count++;
    }
    
    closedir(dir);
    free(path);
    
    printf("========================\n");
    printf("Total: %d items (%d directories, %d files, %s)\n", 
           count, dir_count, file_count, 
           total_size < 1024 ? 
           (total_size == 0 ? "0 B" : "<1 KB") :
           total_size < 1024*1024 ?
           (total_size/1024 == 0 ? "<1 KB" : ">1 KB") :
           ">1 MB");
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
}

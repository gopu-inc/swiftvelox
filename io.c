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
    time_t last_access;
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
        desc->is_open = false;
        fd_count--;
    }
}

static void update_fd_access(int fd) {
    FileDescriptor* desc = get_fd(fd);
    if (desc) {
        desc->last_access = time(NULL);
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
        } else if (node->type == NODE_NULL) {
            strcpy(result, "null");
        } else if (node->type == NODE_UNDEFINED) {
            strcpy(result, "undefined");
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
                                "rb", "wb", "ab", "r+b", "w+b", "a+b", 
                                "rt", "wt", "at", "r+t", "w+t", "a+t", NULL};
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
    
    // Vérifier si le fichier existe pour les modes lecture
    if (strchr(mode, 'r') && !strchr(mode, '+')) {
        if (access(filename, F_OK) != 0) {
            printf("%s[IO ERROR]%s File does not exist: %s\n", 
                   COLOR_RED, COLOR_RESET, filename);
            free(filename);
            free(mode);
            if (var_name) free(var_name);
            return;
        }
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
        printf("%s[IO ERROR]%s Too many open files (max 256)\n", COLOR_RED, COLOR_RESET);
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
    desc->last_access = time(NULL);
    
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
        // TODO: Implémenter le stockage réel dans une variable
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
        } else if (temp_size > 65536) {
            printf("%s[IO WARNING]%s Reading size limited to 64KB\n", COLOR_YELLOW, COLOR_RESET);
            size = 65536;
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
    update_fd_access(fd);
    
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
        printf("%s[IO ERROR]%s Invalid data to write\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    size_t bytes_written = fwrite(data, 1, strlen(data), desc->handle);
    desc->position = ftell(desc->handle);
    update_fd_access(fd);
    
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
    
    // Validation de la position
    if (whence == SEEK_SET && offset < 0) {
        printf("%s[IO ERROR]%s Invalid position: %ld (cannot be negative with SEEK_SET)\n", 
               COLOR_RED, COLOR_RESET, offset);
        return;
    }
    
    if (fseek(desc->handle, offset, whence) != 0) {
        printf("%s[IO ERROR]%s Seek failed: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
        return;
    }
    
    desc->position = ftell(desc->handle);
    update_fd_access(fd);
    
    printf("%s[IO]%s Seek to position %ld (whence=%d) on fd=%d, new position=%ld\n", 
           COLOR_GREEN, COLOR_RESET, offset, whence, fd, desc->position);
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
    update_fd_access(fd);
    
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

void io_flush(ASTNode* node) {
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
    
    if (fflush(desc->handle) == 0) {
        printf("%s[IO]%s Flushed buffer for fd=%d\n", COLOR_GREEN, COLOR_RESET, fd);
        update_fd_access(fd);
    } else {
        printf("%s[IO ERROR]%s Flush failed: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
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
        printf("%s[IO]%s Path '%s' is a file: %s (would store in '%s')\n",
               COLOR_GREEN, COLOR_RESET, path, is_file ? "yes" : "no", var_name);
        free(var_name);
    } else {
        printf("%s is a file: %s\n", path, is_file ? "yes" : "no");
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
        printf("%s[IO]%s Path '%s' is a directory: %s (would store in '%s')\n",
               COLOR_GREEN, COLOR_RESET, path, is_dir ? "yes" : "no", var_name);
        free(var_name);
    } else {
        printf("%s is a directory: %s\n", path, is_dir ? "yes" : "no");
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
    
    int mode = 0755; // Default permissions: rwxr-xr-x
    if (node->right) {
        mode = (int)extract_number(node->right);
    }
    
    if (mkdir(dirname_str, mode) != 0) {
        printf("%s[IO ERROR]%s Cannot create directory: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, dirname_str, strerror(errno));
    } else {
        printf("%s[IO]%s Directory created: %s (mode: %o)\n", 
               COLOR_GREEN, COLOR_RESET, dirname_str, mode);
    }
    
    free(dirname_str);
}

void io_rmdir(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing directory name\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* dirname_str = extract_string(node->left);
    if (!dirname_str) return;
    
    // Vérifier si le répertoire existe
    struct stat st;
    if (stat(dirname_str, &st) != 0) {
        printf("%s[IO ERROR]%s Directory does not exist: %s\n", 
               COLOR_RED, COLOR_RESET, dirname_str);
        free(dirname_str);
        return;
    }
    
    // Vérifier que c'est bien un répertoire
    if (!S_ISDIR(st.st_mode)) {
        printf("%s[IO ERROR]%s Path is not a directory: %s\n", 
               COLOR_RED, COLOR_RESET, dirname_str);
        free(dirname_str);
        return;
    }
    
    // Vérifier si le répertoire est vide
    DIR* dir = opendir(dirname_str);
    if (!dir) {
        printf("%s[IO ERROR]%s Cannot open directory: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, dirname_str, strerror(errno));
        free(dirname_str);
        return;
    }
    
    int item_count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
            item_count++;
            break; // On a trouvé un élément, on peut arrêter
        }
    }
    closedir(dir);
    
    if (item_count > 0) {
        printf("%s[IO ERROR]%s Directory is not empty: %s\n", 
               COLOR_RED, COLOR_RESET, dirname_str);
        free(dirname_str);
        return;
    }
    
    if (rmdir(dirname_str) != 0) {
        printf("%s[IO ERROR]%s Cannot remove directory: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, dirname_str, strerror(errno));
    } else {
        printf("%s[IO]%s Directory removed: %s\n", COLOR_GREEN, COLOR_RESET, dirname_str);
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
    
    bool show_all = false;
    bool show_details = true;
    
    // Options optionnelles
    if (node->right) {
        char* options = extract_string(node->right);
        if (options) {
            if (strstr(options, "-a")) show_all = true;
            if (strstr(options, "-l")) show_details = true;
            free(options);
        }
    }
    
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
    int link_count = 0;
    long long total_size = 0;
    
    while ((entry = readdir(dir)) != NULL) {
        if (!show_all && (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0)) {
            continue;
        }
        
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        
        struct stat st;
        const char* type = "?";
        char size_str[32] = "";
        char time_str[64] = "";
        char perm_str[11] = "----------";
        
        if (stat(fullpath, &st) == 0) {
            // Permissions
            if (S_ISDIR(st.st_mode)) {
                type = "D";
                dir_count++;
                perm_str[0] = 'd';
            } else if (S_ISREG(st.st_mode)) {
                type = "F";
                file_count++;
                total_size += st.st_size;
                perm_str[0] = '-';
            } else if (S_ISLNK(st.st_mode)) {
                type = "L";
                link_count++;
                perm_str[0] = 'l';
            }
            
            // Permissions détaillées
            perm_str[1] = (st.st_mode & S_IRUSR) ? 'r' : '-';
            perm_str[2] = (st.st_mode & S_IWUSR) ? 'w' : '-';
            perm_str[3] = (st.st_mode & S_IXUSR) ? 'x' : '-';
            perm_str[4] = (st.st_mode & S_IRGRP) ? 'r' : '-';
            perm_str[5] = (st.st_mode & S_IWGRP) ? 'w' : '-';
            perm_str[6] = (st.st_mode & S_IXGRP) ? 'x' : '-';
            perm_str[7] = (st.st_mode & S_IROTH) ? 'r' : '-';
            perm_str[8] = (st.st_mode & S_IWOTH) ? 'w' : '-';
            perm_str[9] = (st.st_mode & S_IXOTH) ? 'x' : '-';
            
            // Taille formatée
            if (S_ISREG(st.st_mode)) {
                if (st.st_size < 1024)
                    snprintf(size_str, sizeof(size_str), "%lld B", (long long)st.st_size);
                else if (st.st_size < 1024*1024)
                    snprintf(size_str, sizeof(size_str), "%.1f KB", st.st_size/1024.0);
                else if (st.st_size < 1024*1024*1024)
                    snprintf(size_str, sizeof(size_str), "%.1f MB", st.st_size/(1024.0*1024.0));
                else
                    snprintf(size_str, sizeof(size_str), "%.1f GB", st.st_size/(1024.0*1024.0*1024.0));
            }
            
            // Date de modification
            struct tm* tm_info = localtime(&st.st_mtime);
            strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M", tm_info);
        }
        
        if (show_details) {
            if (S_ISDIR(st.st_mode)) {
                printf("  %s \033[1;34m%s/\033[0m %8s  %s\n", 
                       perm_str, entry->d_name, size_str, time_str);
            } else if (S_ISLNK(st.st_mode)) {
                printf("  %s \033[1;36m%s\033[0m %8s  %s\n", 
                       perm_str, entry->d_name, size_str, time_str);
            } else if (S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR || st.st_mode & S_IXGRP || st.st_mode & S_IXOTH)) {
                printf("  %s \033[1;32m%s\033[0m %8s  %s\n", 
                       perm_str, entry->d_name, size_str, time_str);
            } else {
                printf("  %s %-30s %8s  %s\n", 
                       perm_str, entry->d_name, size_str, time_str);
            }
        } else {
            if (S_ISDIR(st.st_mode)) {
                printf("  \033[1;34m%s/\033[0m\n", entry->d_name);
            } else if (S_ISLNK(st.st_mode)) {
                printf("  \033[1;36m%s\033[0m\n", entry->d_name);
            } else if (S_ISREG(st.st_mode) && (st.st_mode & S_IXUSR || st.st_mode & S_IXGRP || st.st_mode & S_IXOTH)) {
                printf("  \033[1;32m%s\033[0m\n", entry->d_name);
            } else {
                printf("  %s\n", entry->d_name);
            }
        }
        
        count++;
    }
    
    closedir(dir);
    free(path);
    
    printf("========================\n");
    printf("Total: %d items (%d directories, %d files, %d links", 
           count, dir_count, file_count, link_count);
    
    if (total_size > 0) {
        char total_size_str[32];
        if (total_size < 1024)
            snprintf(total_size_str, sizeof(total_size_str), "%lld B", total_size);
        else if (total_size < 1024*1024)
            snprintf(total_size_str, sizeof(total_size_str), "%.1f KB", total_size/1024.0);
        else if (total_size < 1024*1024*1024)
            snprintf(total_size_str, sizeof(total_size_str), "%.1f MB", total_size/(1024.0*1024.0));
        else
            snprintf(total_size_str, sizeof(total_size_str), "%.1f GB", total_size/(1024.0*1024.0*1024.0));
        
        printf(", total size: %s", total_size_str);
    }
    printf(")\n");
}

void io_remove(ASTNode* node) {
    if (!node || !node->left) {
        printf("%s[IO ERROR]%s Missing filename\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* filename = extract_string(node->left);
    if (!filename) return;
    
    // Vérifier si le fichier existe
    if (access(filename, F_OK) != 0) {
        printf("%s[IO ERROR]%s File does not exist: %s\n", 
               COLOR_RED, COLOR_RESET, filename);
        free(filename);
        return;
    }
    
    // Vérifier que c'est un fichier (pas un répertoire)
    struct stat st;
    if (stat(filename, &st) == 0 && S_ISDIR(st.st_mode)) {
        printf("%s[IO ERROR]%s Path is a directory (use io.rmdir): %s\n", 
               COLOR_RED, COLOR_RESET, filename);
        free(filename);
        return;
    }
    
    if (remove(filename) == 0) {
        printf("%s[IO]%s File removed: %s\n", COLOR_GREEN, COLOR_RESET, filename);
    } else {
        printf("%s[IO ERROR]%s Cannot remove file: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, filename, strerror(errno));
    }
    
    free(filename);
}

void io_rename(ASTNode* node) {
    if (!node || !node->left || !node->right) {
        printf("%s[IO ERROR]%s Missing source or destination filename\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* oldname = extract_string(node->left);
    char* newname = extract_string(node->right);
    
    if (!oldname || !newname) {
        if (oldname) free(oldname);
        if (newname) free(newname);
        return;
    }
    
    // Vérifier si l'ancien fichier existe
    if (access(oldname, F_OK) != 0) {
        printf("%s[IO ERROR]%s Source file does not exist: %s\n", 
               COLOR_RED, COLOR_RESET, oldname);
        free(oldname);
        free(newname);
        return;
    }
    
    // Vérifier si le nouveau nom existe déjà
    if (access(newname, F_OK) == 0) {
        printf("%s[IO WARNING]%s Destination file already exists: %s\n", 
               COLOR_YELLOW, COLOR_RESET, newname);
    }
    
    if (rename(oldname, newname) == 0) {
        printf("%s[IO]%s File renamed: %s -> %s\n", 
               COLOR_GREEN, COLOR_RESET, oldname, newname);
    } else {
        printf("%s[IO ERROR]%s Cannot rename file: %s -> %s (%s)\n", 
               COLOR_RED, COLOR_RESET, oldname, newname, strerror(errno));
    }
    
    free(oldname);
    free(newname);
}

void io_copy(ASTNode* node) {
    if (!node || !node->left || !node->right) {
        printf("%s[IO ERROR]%s Missing source or destination filename\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* srcname = extract_string(node->left);
    char* dstname = extract_string(node->right);
    
    if (!srcname || !dstname) {
        if (srcname) free(srcname);
        if (dstname) free(dstname);
        return;
    }
    
    // Vérifier si la source existe
    if (access(srcname, F_OK) != 0) {
        printf("%s[IO ERROR]%s Source file does not exist: %s\n", 
               COLOR_RED, COLOR_RESET, srcname);
        free(srcname);
        free(dstname);
        return;
    }
    
    // Vérifier que la source est un fichier
    struct stat st;
    if (stat(srcname, &st) != 0 || S_ISDIR(st.st_mode)) {
        printf("%s[IO ERROR]%s Source is not a regular file: %s\n", 
               COLOR_RED, COLOR_RESET, srcname);
        free(srcname);
        free(dstname);
        return;
    }
    
    // Ouvrir la source
    FILE* src = fopen(srcname, "rb");
    if (!src) {
        printf("%s[IO ERROR]%s Cannot open source file: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, srcname, strerror(errno));
        free(srcname);
        free(dstname);
        return;
    }
    
    // Ouvrir la destination
    FILE* dst = fopen(dstname, "wb");
    if (!dst) {
        printf("%s[IO ERROR]%s Cannot open destination file: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, dstname, strerror(errno));
        fclose(src);
        free(srcname);
        free(dstname);
        return;
    }
    
    // Copier le contenu
    char buffer[8192];
    size_t bytes_read, total_bytes = 0;
    
    while ((bytes_read = fread(buffer, 1, sizeof(buffer), src)) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_read, dst);
        if (bytes_written != bytes_read) {
            printf("%s[IO ERROR]%s Write error during copy\n", COLOR_RED, COLOR_RESET);
            fclose(src);
            fclose(dst);
            free(srcname);
            free(dstname);
            return;
        }
        total_bytes += bytes_written;
    }
    
    fclose(src);
    fclose(dst);
    
    printf("%s[IO]%s File copied: %s -> %s (%zu bytes)\n", 
           COLOR_GREEN, COLOR_RESET, srcname, dstname, total_bytes);
    
    free(srcname);
    free(dstname);
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
    file_descriptors[0].size = 0;
    file_descriptors[0].last_access = time(NULL);
    
    file_descriptors[1].id = 1;
    file_descriptors[1].name = str_copy("stdout");
    file_descriptors[1].handle = stdout;
    file_descriptors[1].mode = str_copy("w");
    file_descriptors[1].is_open = true;
    file_descriptors[1].position = 0;
    file_descriptors[1].size = 0;
    file_descriptors[1].last_access = time(NULL);
    
    file_descriptors[2].id = 2;
    file_descriptors[2].name = str_copy("stderr");
    file_descriptors[2].handle = stderr;
    file_descriptors[2].mode = str_copy("w");
    file_descriptors[2].is_open = true;
    file_descriptors[2].position = 0;
    file_descriptors[2].size = 0;
    file_descriptors[2].last_access = time(NULL);
    
    fd_count = 3;
    
    printf("%s[IO MODULE]%s Initialized with %d file descriptors\n", 
           COLOR_GREEN, COLOR_RESET, fd_count);
}
// Ajoute ceci à la fin de io.c

// Vérifie si un fichier existe et retourne true/false
bool io_exists_bool(const char* path) {
    if (!path) return false;
    // access() retourne 0 si le fichier existe
    return (access(path, F_OK) == 0);
}

// Lit tout le contenu d'un fichier et le retourne sous forme de string
char* io_read_string(const char* path) {
    if (!path) return NULL;
    
    FILE* f = fopen(path, "rb"); // "rb" pour lire aussi les fichiers binaires/images
    if (!f) return NULL;
    
    // Calculer la taille
    fseek(f, 0, SEEK_END);
    long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    // Allouer la mémoire (+1 pour le caractère de fin de chaîne \0)
    char* buffer = malloc(length + 1);
    if (buffer) {
        fread(buffer, 1, length, f);
        buffer[length] = '\0';
    }
    
    fclose(f);
    return buffer;
}


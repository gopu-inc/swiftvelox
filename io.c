// io.c
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <errno.h>
#include "common.h"

// ======================================================
// [SECTION] GESTION DES DESCRIPTEURS DE FICHIER
// ======================================================
typedef enum {
    FD_FILE,
    FD_DIRECTORY,
    FD_SOCKET,
    FD_PIPE
} FileDescriptorType;

typedef struct {
    int id;
    char* name;
    FILE* handle;
    FileDescriptorType type;
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
// [SECTION] FONCTIONS D'EXÉCUTION IO
// ======================================================
void io_open(ASTNode* node) {
    if (!node->left || !node->right) {
        printf("%s[IO ERROR]%s Missing filename or mode\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* filename = evalString(node->left);
    char* mode = evalString(node->right);
    
    if (!filename || !mode) {
        if (filename) free(filename);
        if (mode) free(mode);
        return;
    }
    
    // Valider le mode
    bool valid_mode = false;
    const char* valid_modes[] = {"r", "w", "a", "r+", "w+", "a+", "rb", "wb", "ab", "r+b", "w+b", "a+b", NULL};
    
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
        return;
    }
    
    FILE* f = fopen(filename, mode);
    if (!f) {
        printf("%s[IO ERROR]%s Cannot open file: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, filename, strerror(errno));
        free(filename);
        free(mode);
        return;
    }
    
    int fd = allocate_fd();
    if (fd == -1) {
        printf("%s[IO ERROR]%s Too many open files\n", COLOR_RED, COLOR_RESET);
        fclose(f);
        free(filename);
        free(mode);
        return;
    }
    
    FileDescriptor* desc = &file_descriptors[fd];
    desc->name = str_copy(filename);
    desc->handle = f;
    desc->type = FD_FILE;
    desc->mode = str_copy(mode);
    desc->is_open = true;
    
    // Obtenir la taille du fichier
    fseek(f, 0, SEEK_END);
    desc->size = ftell(f);
    fseek(f, 0, SEEK_SET);
    desc->position = 0;
    
    // Stocker le fd dans une variable si demandé
    if (node->third) {
        char* var_name = evalString(node->third);
        if (var_name) {
            int var_idx = findVar(var_name);
            if (var_idx == -1 && var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, var_name, 99);
                var->name[99] = '\0';
                var->type = TK_VAR;
                var->size_bytes = sizeof(int);
                var->scope_level = scope_level;
                var->is_constant = false;
                var->is_initialized = true;
                var->is_string = false;
                var->is_float = false;
                var->value.int_val = fd;
                var_count++;
            } else if (var_idx >= 0) {
                vars[var_idx].value.int_val = fd;
                vars[var_idx].is_initialized = true;
            }
            free(var_name);
        }
    }
    
    printf("%s[IO]%s File opened: %s (fd=%d, mode=%s, size=%d)\n", 
           COLOR_GREEN, COLOR_RESET, filename, fd, mode, desc->size);
    
    free(filename);
    free(mode);
}

void io_close(ASTNode* node) {
    if (!node->left) {
        printf("%s[IO ERROR]%s Missing file descriptor\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = evalFloat(node->left);
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
    if (!node->left) {
        printf("%s[IO ERROR]%s Missing file descriptor\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = evalFloat(node->left);
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
    
    // Vérifier si on peut lire
    if (strchr(desc->mode, 'r') == NULL && strchr(desc->mode, '+') == NULL) {
        printf("%s[IO ERROR]%s File not opened for reading: %s\n", 
               COLOR_RED, COLOR_RESET, desc->mode);
        return;
    }
    
    size_t size = 1024; // Taille par défaut
    if (node->right) {
        size = (size_t)evalFloat(node->right);
        if (size <= 0) size = 1024;
        if (size > 65536) size = 65536; // Limite
    }
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        printf("%s[IO ERROR]%s Memory allocation failed\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    size_t bytes_read = fread(buffer, 1, size, desc->handle);
    buffer[bytes_read] = '\0';
    
    desc->position = ftell(desc->handle);
    
    // Stocker le résultat
    if (node->third) {
        char* var_name = evalString(node->third);
        if (var_name) {
            int var_idx = findVar(var_name);
            if (var_idx == -1 && var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, var_name, 99);
                var->name[99] = '\0';
                var->type = TK_VAR;
                var->size_bytes = bytes_read + 1;
                var->scope_level = scope_level;
                var->is_constant = false;
                var->is_initialized = true;
                var->is_string = true;
                var->is_float = false;
                var->value.str_val = str_copy(buffer);
                var_count++;
            } else if (var_idx >= 0) {
                if (vars[var_idx].value.str_val) free(vars[var_idx].value.str_val);
                vars[var_idx].value.str_val = str_copy(buffer);
                vars[var_idx].is_string = true;
                vars[var_idx].is_initialized = true;
            }
            free(var_name);
        }
    } else {
        // Afficher le contenu
        printf("%s", buffer);
    }
    
    printf("%s[IO]%s Read %zu bytes from fd=%d\n", 
           COLOR_GREEN, COLOR_RESET, bytes_read, fd);
    
    free(buffer);
}

void io_write(ASTNode* node) {
    if (!node->left || !node->right) {
        printf("%s[IO ERROR]%s Missing file descriptor or data\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = evalFloat(node->left);
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
    
    // Vérifier si on peut écrire
    if (strchr(desc->mode, 'w') == NULL && strchr(desc->mode, 'a') == NULL && 
        strchr(desc->mode, '+') == NULL) {
        printf("%s[IO ERROR]%s File not opened for writing: %s\n", 
               COLOR_RED, COLOR_RESET, desc->mode);
        return;
    }
    
    char* data = evalString(node->right);
    if (!data) {
        return;
    }
    
    size_t bytes_written = fwrite(data, 1, strlen(data), desc->handle);
    desc->position = ftell(desc->handle);
    
    if (bytes_written != strlen(data)) {
        printf("%s[IO WARNING]%s Partial write: %zu/%zu bytes\n", 
               COLOR_YELLOW, COLOR_RESET, bytes_written, strlen(data));
    }
    
    printf("%s[IO]%s Wrote %zu bytes to fd=%d\n", 
           COLOR_GREEN, COLOR_RESET, bytes_written, fd);
    
    free(data);
}

void io_seek(ASTNode* node) {
    if (!node->left || !node->right) {
        printf("%s[IO ERROR]%s Missing file descriptor or position\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = evalFloat(node->left);
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
    
    int whence = SEEK_SET; // Par défaut
    if (node->third) {
        char* whence_str = evalString(node->third);
        if (whence_str) {
            if (strcmp(whence_str, "cur") == 0 || strcmp(whence_str, "current") == 0) {
                whence = SEEK_CUR;
            } else if (strcmp(whence_str, "end") == 0) {
                whence = SEEK_END;
            }
            free(whence_str);
        }
    }
    
    long offset = (long)evalFloat(node->right);
    
    if (fseek(desc->handle, offset, whence) != 0) {
        printf("%s[IO ERROR]%s Seek failed: %s\n", COLOR_RED, COLOR_RESET, strerror(errno));
        return;
    }
    
    desc->position = ftell(desc->handle);
    
    printf("%s[IO]%s Seek to position %ld (whence=%d) on fd=%d\n", 
           COLOR_GREEN, COLOR_RESET, offset, whence, fd);
}

void io_tell(ASTNode* node) {
    if (!node->left) {
        printf("%s[IO ERROR]%s Missing file descriptor\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    double fd_val = evalFloat(node->left);
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
    
    // Stocker le résultat
    if (node->right) {
        char* var_name = evalString(node->right);
        if (var_name) {
            int var_idx = findVar(var_name);
            if (var_idx == -1 && var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, var_name, 99);
                var->name[99] = '\0';
                var->type = TK_VAR;
                var->size_bytes = sizeof(long);
                var->scope_level = scope_level;
                var->is_constant = false;
                var->is_initialized = true;
                var->is_string = false;
                var->is_float = false;
                var->value.int_val = pos;
                var_count++;
            } else if (var_idx >= 0) {
                vars[var_idx].value.int_val = pos;
                vars[var_idx].is_initialized = true;
            }
            free(var_name);
        }
    } else {
        printf("Position: %ld\n", pos);
    }
}

// ======================================================
// [SECTION] FONCTIONS DE SYSTÈME DE FICHIERS
// ======================================================
void io_exists(ASTNode* node) {
    if (!node->left) {
        printf("%s[IO ERROR]%s Missing path\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* path = evalString(node->left);
    if (!path) return;
    
    bool exists = (access(path, F_OK) == 0);
    
    if (node->right) {
        char* var_name = evalString(node->right);
        if (var_name) {
            int var_idx = findVar(var_name);
            if (var_idx == -1 && var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, var_name, 99);
                var->name[99] = '\0';
                var->type = TK_VAR;
                var->size_bytes = sizeof(bool);
                var->scope_level = scope_level;
                var->is_constant = false;
                var->is_initialized = true;
                var->is_string = false;
                var->is_float = false;
                var->value.int_val = exists ? 1 : 0;
                var_count++;
            } else if (var_idx >= 0) {
                vars[var_idx].value.int_val = exists ? 1 : 0;
                vars[var_idx].is_initialized = true;
            }
            free(var_name);
        }
    } else {
        printf("%s exists: %s\n", path, exists ? "yes" : "no");
    }
    
    free(path);
}

void io_isfile(ASTNode* node) {
    if (!node->left) {
        printf("%s[IO ERROR]%s Missing path\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* path = evalString(node->left);
    if (!path) return;
    
    struct stat st;
    bool is_file = (stat(path, &st) == 0 && S_ISREG(st.st_mode));
    
    if (node->right) {
        char* var_name = evalString(node->right);
        if (var_name) {
            int var_idx = findVar(var_name);
            if (var_idx == -1 && var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, var_name, 99);
                var->name[99] = '\0';
                var->type = TK_VAR;
                var->size_bytes = sizeof(bool);
                var->scope_level = scope_level;
                var->is_constant = false;
                var->is_initialized = true;
                var->is_string = false;
                var->is_float = false;
                var->value.int_val = is_file ? 1 : 0;
                var_count++;
            } else if (var_idx >= 0) {
                vars[var_idx].value.int_val = is_file ? 1 : 0;
                vars[var_idx].is_initialized = true;
            }
            free(var_name);
        }
    }
    
    free(path);
}

void io_isdir(ASTNode* node) {
    if (!node->left) {
        printf("%s[IO ERROR]%s Missing path\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* path = evalString(node->left);
    if (!path) return;
    
    struct stat st;
    bool is_dir = (stat(path, &st) == 0 && S_ISDIR(st.st_mode));
    
    if (node->right) {
        char* var_name = evalString(node->right);
        if (var_name) {
            int var_idx = findVar(var_name);
            if (var_idx == -1 && var_count < 1000) {
                Variable* var = &vars[var_count];
                strncpy(var->name, var_name, 99);
                var->name[99] = '\0';
                var->type = TK_VAR;
                var->size_bytes = sizeof(bool);
                var->scope_level = scope_level;
                var->is_constant = false;
                var->is_initialized = true;
                var->is_string = false;
                var->is_float = false;
                var->value.int_val = is_dir ? 1 : 0;
                var_count++;
            } else if (var_idx >= 0) {
                vars[var_idx].value.int_val = is_dir ? 1 : 0;
                vars[var_idx].is_initialized = true;
            }
            free(var_name);
        }
    }
    
    free(path);
}

void io_mkdir(ASTNode* node) {
    if (!node->left) {
        printf("%s[IO ERROR]%s Missing directory name\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* dirname = evalString(node->left);
    if (!dirname) return;
    
    int mode = 0755; // Mode par défaut
    if (node->right) {
        mode = (int)evalFloat(node->right);
    }
    
    if (mkdir(dirname, mode) != 0) {
        printf("%s[IO ERROR]%s Cannot create directory: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, dirname, strerror(errno));
    } else {
        printf("%s[IO]%s Directory created: %s\n", COLOR_GREEN, COLOR_RESET, dirname);
    }
    
    free(dirname);
}

void io_listdir(ASTNode* node) {
    if (!node->left) {
        printf("%s[IO ERROR]%s Missing directory path\n", COLOR_RED, COLOR_RESET);
        return;
    }
    
    char* path = evalString(node->left);
    if (!path) return;
    
    DIR* dir = opendir(path);
    if (!dir) {
        printf("%s[IO ERROR]%s Cannot open directory: %s (%s)\n", 
               COLOR_RED, COLOR_RESET, path, strerror(errno));
        free(path);
        return;
    }
    
    printf("%s[IO]%s Contents of %s:\n", COLOR_GREEN, COLOR_RESET, path);
    
    struct dirent* entry;
    int count = 0;
    while ((entry = readdir(dir)) != NULL) {
        // Ignorer . et ..
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }
        
        char fullpath[PATH_MAX];
        snprintf(fullpath, sizeof(fullpath), "%s/%s", path, entry->d_name);
        
        struct stat st;
        const char* type = "?";
        if (stat(fullpath, &st) == 0) {
            if (S_ISREG(st.st_mode)) type = "F";
            else if (S_ISDIR(st.st_mode)) type = "D";
            else if (S_ISLNK(st.st_mode)) type = "L";
        }
        
        printf("  [%s] %s\n", type, entry->d_name);
        count++;
    }
    
    printf("Total: %d items\n", count);
    
    closedir(dir);
    free(path);
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
    file_descriptors[0].type = FD_FILE;
    file_descriptors[0].mode = str_copy("r");
    file_descriptors[0].is_open = true;
    file_descriptors[0].position = 0;
    fd_count++;
    
    file_descriptors[1].id = 1;
    file_descriptors[1].name = str_copy("stdout");
    file_descriptors[1].handle = stdout;
    file_descriptors[1].type = FD_FILE;
    file_descriptors[1].mode = str_copy("w");
    file_descriptors[1].is_open = true;
    file_descriptors[1].position = 0;
    fd_count++;
    
    file_descriptors[2].id = 2;
    file_descriptors[2].name = str_copy("stderr");
    file_descriptors[2].handle = stderr;
    file_descriptors[2].type = FD_FILE;
    file_descriptors[2].mode = str_copy("w");
    file_descriptors[2].is_open = true;
    file_descriptors[2].position = 0;
    fd_count++;
    
    printf("%s[IO MODULE]%s Initialized with %d file descriptors\n", 
           COLOR_GREEN, COLOR_RESET, fd_count);
}

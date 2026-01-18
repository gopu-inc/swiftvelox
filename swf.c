#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include "common.h"

extern ASTNode** parse(const char* source, int* count);

// ======================================================
// [SECTION] VM STATE
// ======================================================
typedef struct {
    char name[50];
    union {
        int int_val;
        float float_val;
    } value;
    bool is_float;
} Variable;

typedef struct {
    Variable vars[100];
    int count;
    char* import_path;
    char* current_package;
} VM;

static VM vm = {0};

// ======================================================
// [SECTION] PACKAGE EXPORT SYSTEM
// ======================================================
typedef struct {
    char alias[50];
    char module_path[100];
} PackageExport;

typedef struct {
    char name[50];
    PackageExport exports[20];
    int export_count;
} Package;

static Package packages[20];
static int package_count = 0;

static void registerPackageExport(const char* package_name, const char* alias, const char* module_path) {
    // Trouver ou créer le package
    int pkg_idx = -1;
    for (int i = 0; i < package_count; i++) {
        if (strcmp(packages[i].name, package_name) == 0) {
            pkg_idx = i;
            break;
        }
    }
    
    if (pkg_idx == -1) {
        // Créer un nouveau package
        if (package_count < 20) {
            pkg_idx = package_count++;
            strncpy(packages[pkg_idx].name, package_name, 49);
            packages[pkg_idx].name[49] = '\0';
            packages[pkg_idx].export_count = 0;
        } else {
            printf(RED "[ERROR]" RESET " Too many packages\n");
            return;
        }
    }
    
    // Ajouter l'export
    if (packages[pkg_idx].export_count < 20) {
        PackageExport* exp = &packages[pkg_idx].exports[packages[pkg_idx].export_count++];
        strncpy(exp->alias, alias, 49);
        exp->alias[49] = '\0';
        strncpy(exp->module_path, module_path, 99);
        exp->module_path[99] = '\0';
        
        printf(CYAN "[PACKAGE]" RESET " %s exports '%s' as '%s'\n", 
               package_name, module_path, alias);
    }
}

static char* resolvePackageExport(const char* alias, const char* package_name) {
    for (int i = 0; i < package_count; i++) {
        if (strcmp(packages[i].name, package_name) == 0) {
            for (int j = 0; j < packages[i].export_count; j++) {
                if (strcmp(packages[i].exports[j].alias, alias) == 0) {
                    // Construire le chemin complet
                    char* full_path = malloc(strlen("/usr/local/lib/swift/") + 
                                            strlen(package_name) + 
                                            strlen(packages[i].exports[j].module_path) + 10);
                    sprintf(full_path, "/usr/local/lib/swift/%s/%s", 
                            package_name, packages[i].exports[j].module_path);
                    return full_path;
                }
            }
        }
    }
    return NULL;
}

// ======================================================
// [SECTION] HELPER FUNCTIONS
// ======================================================
static char* my_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s);
    char* d = malloc(len + 1);
    if (d) strcpy(d, s);
    return d;
}

static void setImportPath(const char* path) {
    if (vm.import_path) free(vm.import_path);
    vm.import_path = my_strdup(path);
}

static void setCurrentPackage(const char* package) {
    if (vm.current_package) free(vm.current_package);
    vm.current_package = my_strdup(package);
}

static char* findImportFile(const char* module, const char* from_package) {
    // DEBUG: Afficher ce qu'on cherche
    printf(YELLOW "[FIND]" RESET " Looking for: '%s'", module);
    if (from_package) printf(" from package: '%s'", from_package);
    if (vm.current_package) printf(" (current package: '%s')", vm.current_package);
    printf("\n");
    
    // 1. Si chemin direct (commence par . ou /)
    if (module[0] == '.' || module[0] == '/') {
        // Si c'est un chemin relatif (commence par .) ET qu'on est dans un package
        if (module[0] == '.' && vm.current_package) {
            printf(CYAN "[PACKAGE]" RESET " Resolving relative path in package %s: %s\n", 
                   vm.current_package, module);
            
            // Construire le chemin absolu dans le package
            char* full_path = malloc(strlen("/usr/local/lib/swift/") + 
                                    strlen(vm.current_package) + 
                                    strlen(module) + 10);
            if (!full_path) return NULL;
            
            // Si c'est "./fichier", enlever le "./"
            const char* actual_module = module;
            if (strncmp(module, "./", 2) == 0) {
                actual_module = module + 2;
            } else if (strncmp(module, "../", 3) == 0) {
                // Pour l'instant, on ne gère pas ../
                printf(RED "[ERROR]" RESET " Parent directory '..' not supported in package imports\n");
                free(full_path);
                return NULL;
            }
            
            sprintf(full_path, "/usr/local/lib/swift/%s/%s", 
                    vm.current_package, actual_module);
            
            printf(CYAN "[PACKAGE]" RESET " Full path: %s\n", full_path);
            return full_path;
        }
        
        // Sinon, chemin normal
        return my_strdup(module);
    }
    
    // 2. Si from_package est spécifié, essayer de résoudre l'export
    if (from_package) {
        char* resolved = resolvePackageExport(module, from_package);
        if (resolved) {
            printf(CYAN "[PACKAGE]" RESET " Resolved export %s -> %s from package %s\n", 
                   module, resolved, from_package);
            return resolved;
        }
    }
    
    // 3. Déterminer le chemin de base
    const char* base_path = "/usr/local/lib/swift/";
    
    // 4. Chercher comme package (dossier)
    char* package_dir = malloc(strlen(base_path) + strlen(module) + 2);
    sprintf(package_dir, "%s%s/", base_path, module);
    
    struct stat st;
    if (stat(package_dir, &st) == 0 && S_ISDIR(st.st_mode)) {
        // C'est un dossier package, chercher le fichier .svlib
        char* svlib_path = malloc(strlen(package_dir) + strlen(module) + 10);
        sprintf(svlib_path, "%s%s.svlib", package_dir, module);
        
        if (access(svlib_path, F_OK) == 0) {
            free(package_dir);
            printf(CYAN "[PACKAGE]" RESET " Found package: %s\n", module);
            return svlib_path;
        }
        free(svlib_path);
    }
    free(package_dir);
    
    // 5. Chercher comme fichier simple
    char* path = malloc(strlen(base_path) + strlen(module) + 10);
    
    // Essayer avec .swf
    sprintf(path, "%s%s.swf", base_path, module);
    if (access(path, F_OK) == 0) {
        return path;
    }
    
    // Essayer sans extension
    sprintf(path, "%s%s", base_path, module);
    if (access(path, F_OK) == 0) {
        return path;
    }
    
    // 6. Si on est dans un package, essayer dans le package courant
    if (vm.current_package) {
        sprintf(path, "/usr/local/lib/swift/%s/%s", vm.current_package, module);
        if (access(path, F_OK) == 0) {
            return path;
        }
        
        sprintf(path, "/usr/local/lib/swift/%s/%s.swf", vm.current_package, module);
        if (access(path, F_OK) == 0) {
            return path;
        }
    }
    
    free(path);
    return NULL;
}

static void importModule(const char* module, const char* from_package);
static void run(const char* source, const char* filename);

static void importModule(const char* module, const char* from_package) {
    printf(YELLOW "[IMPORT]" RESET " Loading: %s", module);
    if (from_package) printf(" from package: %s", from_package);
    printf("\n");
    
    char* path = findImportFile(module, from_package);
    if (!path) {
        printf(RED "[ERROR]" RESET " Cannot find module: %s\n", module);
        return;
    }
    
    FILE* f = fopen(path, "r");
    if (!f) {
        printf(RED "[ERROR]" RESET " Cannot open: %s\n", path);
        free(path);
        return;
    }
    
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    
    char* source = malloc(size + 1);
    fread(source, 1, size, f);
    source[size] = '\0';
    fclose(f);
    
    // Vérifier si c'est un fichier .svlib (package manifest)
    const char* ext = strrchr(path, '.');
    bool is_package = ext && strcmp(ext, ".svlib") == 0;
    
    // Sauvegarder ancien état
    char* old_path = vm.import_path ? my_strdup(vm.import_path) : NULL;
    char* old_package = vm.current_package ? my_strdup(vm.current_package) : NULL;
    
    // Définir nouveau chemin et package
    char* dir_path = my_strdup(path);
    char* last_slash = strrchr(dir_path, '/');
    if (last_slash) {
        if (is_package) {
            // Pour un package, extraire le nom du package du chemin
            *last_slash = '\0'; // Enlever le fichier .svlib
            char* package_name = strrchr(dir_path, '/');
            if (package_name) {
                setCurrentPackage(package_name + 1);
            } else {
                setCurrentPackage(dir_path);
            }
        } else {
            *last_slash = '\0';
        }
        setImportPath(dir_path);
    }
    free(dir_path);
    
    // Exécuter le module/package
    if (is_package) {
        printf(CYAN "[PACKAGE]" RESET " Executing package manifest: %s\n", module);
    }
    run(source, module);
    
    // Restaurer ancien état
    if (old_path) {
        setImportPath(old_path);
        free(old_path);
    } else if (vm.import_path) {
        free(vm.import_path);
        vm.import_path = NULL;
    }
    
    if (old_package) {
        setCurrentPackage(old_package);
        free(old_package);
    } else if (vm.current_package) {
        free(vm.current_package);
        vm.current_package = NULL;
    }
    
    free(source);
    free(path);
}

// ======================================================
// [SECTION] EXECUTION
// ======================================================
static int findVar(const char* name) {
    for (int i = 0; i < vm.count; i++) {
        if (strcmp(vm.vars[i].name, name) == 0) {
            return i;
        }
    }
    return -1;
}

static void evalAndPrint(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_INT:
            printf("%lld", node->data.int_val);
            break;
            
        case NODE_FLOAT:
            printf("%.2f", node->data.float_val);
            break;
            
        case NODE_STRING:
            printf("%s", node->data.str_val);
            break;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vm.vars[idx].is_float) {
                    printf("%.2f", vm.vars[idx].value.float_val);
                } else {
                    printf("%d", vm.vars[idx].value.int_val);
                }
            } else {
                printf(RED "[ERROR]" RESET " Undefined variable: %s\n", node->data.name);
            }
            break;
        }
            
        case NODE_BINARY: {
            // Pour l'affichage simple, on traite seulement les opérations de base
            if (node->left) evalAndPrint(node->left);
            printf(" ");
            switch ((TokenKind)node->data.int_val) {
                case TK_PLUS: printf("+"); break;
                case TK_MINUS: printf("-"); break;
                case TK_MULT: printf("*"); break;
                case TK_DIV: printf("/"); break;
                case TK_EQ: printf("=="); break;
                case TK_NEQ: printf("!="); break;
                default: printf("?"); break;
            }
            printf(" ");
            if (node->right) evalAndPrint(node->right);
            break;
        }
            
        default:
            break;
    }
}

static float evalFloat(ASTNode* node) {
    if (!node) return 0.0f;
    
    switch (node->type) {
        case NODE_INT:
            return (float)node->data.int_val;
            
        case NODE_FLOAT:
            return (float)node->data.float_val;
            
        case NODE_IDENT: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (vm.vars[idx].is_float) {
                    return vm.vars[idx].value.float_val;
                } else {
                    return (float)vm.vars[idx].value.int_val;
                }
            }
            return 0.0f;
        }
            
        case NODE_BINARY: {
            float left = evalFloat(node->left);
            float right = evalFloat(node->right);
            
            switch ((TokenKind)node->data.int_val) {
                case TK_PLUS: return left + right;
                case TK_MINUS: return left - right;
                case TK_MULT: return left * right;
                case TK_DIV: return right != 0 ? left / right : 0;
                default: return 0;
            }
        }
            
        default:
            return 0.0f;
    }
}

static void execute(ASTNode* node) {
    if (!node) return;
    
    switch (node->type) {
        case NODE_VAR: {
            // Si on est dans un package, les variables avec des valeurs string
            // sont considérées comme des exports
            if (vm.current_package && node->left && node->left->type == NODE_STRING) {
                // C'est un export de package
                registerPackageExport(vm.current_package, node->data.name, node->left->data.str_val);
                printf(CYAN "[EXPORT]" RESET " %s exports %s as %s\n", 
                       vm.current_package, node->left->data.str_val, node->data.name);
            } else {
                // Variable normale
                int idx = findVar(node->data.name);
                if (idx >= 0) {
                    printf(RED "[ERROR]" RESET " Variable '%s' already exists\n", node->data.name);
                    return;
                }
                
                if (vm.count < 100) {
                    strncpy(vm.vars[vm.count].name, node->data.name, 49);
                    vm.vars[vm.count].name[49] = '\0';
                    
                    if (node->left) {
                        if (node->left->type == NODE_FLOAT) {
                            vm.vars[vm.count].is_float = true;
                            vm.vars[vm.count].value.float_val = evalFloat(node->left);
                        } else {
                            vm.vars[vm.count].is_float = false;
                            vm.vars[vm.count].value.int_val = (int)evalFloat(node->left);
                        }
                    } else {
                        vm.vars[vm.count].is_float = false;
                        vm.vars[vm.count].value.int_val = 0;
                    }
                    vm.count++;
                }
            }
            break;
        }
            
        case NODE_ASSIGN: {
            int idx = findVar(node->data.name);
            if (idx >= 0) {
                if (node->left) {
                    if (vm.vars[idx].is_float || 
                        (node->left->type == NODE_FLOAT)) {
                        vm.vars[idx].is_float = true;
                        vm.vars[idx].value.float_val = evalFloat(node->left);
                    } else {
                        vm.vars[idx].is_float = false;
                        vm.vars[idx].value.int_val = (int)evalFloat(node->left);
                    }
                }
            } else {
                printf(RED "[ERROR]" RESET " Variable '%s' not found\n", node->data.name);
            }
            break;
        }
            
        case NODE_PRINT: {
            if (node->left) {
                evalAndPrint(node->left);
                printf("\n");
            }
            break;
        }
            
        case NODE_IMPORT: {
            for (int i = 0; i < node->import_count; i++) {
                if (node->data.imports[i]) {
                    importModule(node->data.imports[i], node->from_module);
                }
            }
            break;
        }
            
        case NODE_BLOCK:
            if (node->left) execute(node->left);
            if (node->right) execute(node->right);
            break;
            
        default:
            break;
    }
}

static void run(const char* source, const char* filename) {
    (void)filename; // Supprimer l'avertissement unused parameter
    
    int count = 0;
    ASTNode** nodes = parse(source, &count);
    
    if (nodes) {
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                execute(nodes[i]);
            }
        }
        
        // Cleanup
        for (int i = 0; i < count; i++) {
            if (nodes[i]) {
                if (nodes[i]->type == NODE_IMPORT) {
                    for (int j = 0; j < nodes[i]->import_count; j++) {
                        free(nodes[i]->data.imports[j]);
                    }
                    free(nodes[i]->data.imports);
                    if (nodes[i]->from_module) free(nodes[i]->from_module);
                }
                if (nodes[i]->type == NODE_IDENT || 
                    nodes[i]->type == NODE_VAR || 
                    nodes[i]->type == NODE_ASSIGN) {
                    free(nodes[i]->data.name);
                }
                if (nodes[i]->type == NODE_STRING) {
                    free(nodes[i]->data.str_val);
                }
                free(nodes[i]);
            }
        }
        free(nodes);
    }
}

// ======================================================
// [SECTION] MAIN
// ======================================================
static void repl() {
    printf(GREEN "SwiftFlow" RESET " v1.0\n");
    printf("Type 'exit' to quit\n\n");
    
    setImportPath("./");
    
    char line[256];
    while (1) {
        printf("> ");
        if (!fgets(line, sizeof(line), stdin)) break;
        
        line[strcspn(line, "\n")] = 0;
        if (strcmp(line, "exit") == 0) break;
        
        run(line, "REPL");
    }
    
    if (vm.import_path) free(vm.import_path);
    if (vm.current_package) free(vm.current_package);
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        repl();
    } else {
        FILE* f = fopen(argv[1], "r");
        if (!f) {
            printf(RED "[ERROR]" RESET " Cannot open file: %s\n", argv[1]);
            return 1;
        }
        
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        
        char* source = malloc(size + 1);
        fread(source, 1, size, f);
        source[size] = '\0';
        fclose(f);
        
        printf(GREEN "[SUCCES]" RESET ">> %s\n\n", argv[1]);
        
        // Définir le chemin
        char* file_path = my_strdup(argv[1]);
        char* last_slash = strrchr(file_path, '/');
        if (last_slash) {
            *last_slash = '\0';
            setImportPath(file_path);
        } else {
            setImportPath("./");
        }
        free(file_path);
        
        run(source, argv[1]);
        free(source);
        
        if (vm.import_path) free(vm.import_path);
        if (vm.current_package) free(vm.current_package);
    }
    
    return 0;
}

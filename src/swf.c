#include "common.h"
#include <math.h>

// Portable dirname implementation
static char* portable_dirname(const char* path) {
    if (!path) return str_copy(".");
    
    char* copy = str_copy(path);
    char* last_slash = strrchr(copy, '/');
    
    if (last_slash) {
        if (last_slash == copy) {
            // Root directory
            last_slash[1] = '\0';
        } else {
            // Remove last component
            *last_slash = '\0';
        }
    } else {
        // No slash, current directory
        free(copy);
        return str_copy(".");
    }
    
    return copy;
}

// Helper function pour str_startswith
static bool str_startswith(const char* str, const char* prefix) {
    if (!str || !prefix) return false;
    size_t prefix_len = strlen(prefix);
    return strncmp(str, prefix, prefix_len) == 0;
}

// Basic runtime functions

// Create default configuration
SwiftFlowConfig* config_create_default(void) {
    SwiftFlowConfig* config = ALLOC(SwiftFlowConfig);
    CHECK_ALLOC(config, "Config allocation");
    
    config->verbose = false;
    config->debug = false;
    config->warnings = true;
    config->optimize = true;
    config->interpret = false;
    
    config->input_file = NULL;
    
    config->import_paths = NULL;
    config->import_path_count = 0;
    
    config->stack_size = 1024 * 1024; // 1MB
    config->heap_size = 16 * 1024 * 1024; // 16MB
    
    return config;
}

// Free configuration
void config_free(SwiftFlowConfig* config) {
    if (!config) return;
    
    FREE(config->input_file);
    
    if (config->import_paths) {
        for (int i = 0; i < config->import_path_count; i++) {
            FREE(config->import_paths[i]);
        }
        FREE(config->import_paths);
    }
    
    FREE(config);
}

// Add import path
bool config_add_import_path(SwiftFlowConfig* config, const char* path) {
    if (!config || !path) return false;
    
    config->import_path_count++;
    config->import_paths = REALLOC(config->import_paths, char*, config->import_path_count);
    CHECK_ALLOC(config->import_paths, "Import paths reallocation");
    
    config->import_paths[config->import_path_count - 1] = str_copy(path);
    return true;
}

// Resolve import path
char* config_resolve_import(SwiftFlowConfig* config, const char* module_name, const char* from_file) {
    if (!config || !module_name) return NULL;
    
    char* resolved = NULL;
    
    // Check if it's a standard library module
    if (str_startswith(module_name, "std:")) {
        // Standard library import
        char* stdlib_path = "/usr/local/lib/swift/";
        const char* module_path = module_name + 4; // Skip "std:"
        resolved = str_format("%s%s.swf", stdlib_path, module_path);
        
        if (access(resolved, R_OK) == 0) {
            return resolved;
        }
        
        FREE(resolved);
    }
    
    // Check relative to current file
    if (from_file) {
        char* dir = portable_dirname(from_file);
        
        // Try ./module.swf
        resolved = str_format("%s/%s.swf", dir, module_name);
        if (access(resolved, R_OK) == 0) {
            free(dir);
            return resolved;
        }
        FREE(resolved);
        
        // Try ./module/module.swf
        resolved = str_format("%s/%s/%s.swf", dir, module_name, module_name);
        if (access(resolved, R_OK) == 0) {
            free(dir);
            return resolved;
        }
        FREE(resolved);
        
        free(dir);
    }
    
    // Check import paths
    for (int i = 0; i < config->import_path_count; i++) {
        resolved = str_format("%s/%s.swf", config->import_paths[i], module_name);
        if (access(resolved, R_OK) == 0) {
            return resolved;
        }
        FREE(resolved);
    }
    
    // Check default locations
    const char* default_paths[] = {
        "/usr/local/lib/swift/",
        "/usr/lib/swift/",
        "~/.swift/modules/",
        "./modules/",
        NULL
    };
    
    for (int i = 0; default_paths[i]; i++) {
        resolved = str_format("%s%s.swf", default_paths[i], module_name);
        if (access(resolved, R_OK) == 0) {
            return resolved;
        }
        FREE(resolved);
    }
    
    return NULL;
}

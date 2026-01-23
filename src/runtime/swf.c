#include "common.h"

// Basic runtime functions

// Create default configuration
SwiftFlowConfig* config_create_default(void) {
    SwiftFlowConfig* config = ALLOC(SwiftFlowConfig);
    CHECK_ALLOC(config, "Config allocation");
    
    config->verbose = false;
    config->debug = false;
    config->warnings = true;
    config->optimize = true;
    config->emit_llvm = false;
    config->emit_asm = false;
    config->link = true;
    config->interpret = false;
    
    config->input_file = NULL;
    config->output_file = str_copy("a.out");
    config->output_format = str_copy("exe");
    
    config->import_paths = NULL;
    config->import_path_count = 0;
    
    config->gc_enabled = true;
    config->stack_size = 1024 * 1024; // 1MB
    config->heap_size = 16 * 1024 * 1024; // 16MB
    
    config->optimization_level = 1;
    config->target_arch = str_copy("x64");
    
    return config;
}

// Free configuration
void config_free(SwiftFlowConfig* config) {
    if (!config) return;
    
    FREE(config->input_file);
    FREE(config->output_file);
    FREE(config->output_format);
    FREE(config->target_arch);
    
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
        char* stdlib_path = "/usr/local/share/swiftflow/stdlib/";
        const char* module_path = module_name + 4; // Skip "std:"
        resolved = str_format("%s%s.swf", stdlib_path, module_path);
        
        if (access(resolved, R_OK) == 0) {
            return resolved;
        }
        
        FREE(resolved);
    }
    
    // Check relative to current file
    if (from_file) {
        char* dir = str_copy(from_file);
        char* dirname_part = dirname(dir);
        
        // Try ./module.swf
        resolved = str_format("%s/%s.swf", dirname_part, module_name);
        if (access(resolved, R_OK) == 0) {
            free(dir);
            return resolved;
        }
        FREE(resolved);
        
        // Try ./module/module.swf
        resolved = str_format("%s/%s/%s.swf", dirname_part, module_name, module_name);
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
        "/usr/local/lib/swiftflow/",
        "/usr/lib/swiftflow/",
        "~/.swiftflow/modules/",
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

// Value creation functions
Value value_make_int(int64_t val) {
    Value value;
    value.type = VAL_INT;
    value.as.int_val = val;
    return value;
}

Value value_make_float(double val) {
    Value value;
    value.type = VAL_FLOAT;
    value.as.float_val = val;
    return value;
}

Value value_make_bool(bool val) {
    Value value;
    value.type = VAL_BOOL;
    value.as.bool_val = val;
    return value;
}

Value value_make_string(const char* val) {
    Value value;
    value.type = VAL_STRING;
    value.as.str_val = str_copy(val);
    return value;
}

Value value_make_null(void) {
    Value value;
    value.type = VAL_NULL;
    return value;
}

Value value_make_undefined(void) {
    Value value;
    value.type = VAL_UNDEFINED;
    return value;
}

void value_free(Value* value) {
    if (!value) return;
    
    switch (value->type) {
        case VAL_STRING:
            FREE(value->as.str_val);
            break;
        case VAL_ARRAY:
            if (value->as.array.elements) {
                for (int i = 0; i < value->as.array.count; i++) {
                    value_free(&value->as.array.elements[i]);
                }
                FREE(value->as.array.elements);
            }
            break;
        case VAL_MAP:
            if (value->as.map.keys) {
                for (int i = 0; i < value->as.map.count; i++) {
                    FREE(value->as.map.keys[i]);
                    value_free(&value->as.map.values[i]);
                }
                FREE(value->as.map.keys);
                FREE(value->as.map.values);
            }
            break;
        default:
            break;
    }
}

char* value_to_string(Value value) {
    switch (value.type) {
        case VAL_INT:
            return str_format("%ld", value.as.int_val);
        case VAL_FLOAT:
            return str_format("%g", value.as.float_val);
        case VAL_BOOL:
            return str_copy(value.as.bool_val ? "true" : "false");
        case VAL_STRING:
            return str_format("\"%s\"", value.as.str_val);
        case VAL_NULL:
            return str_copy("null");
        case VAL_UNDEFINED:
            return str_copy("undefined");
        default:
            return str_format("<value type %d>", value.type);
    }
}

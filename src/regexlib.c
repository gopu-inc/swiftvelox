#include "common.h"
#include <pcre.h>

// SwiftFlow regex library using PCRE

typedef struct {
    pcre* pattern;
    pcre_extra* extra;
    char* source;
} Regex;

Value swiftflow_regex_compile(const char* pattern) {
    const char* error;
    int erroffset;
    
    pcre* re = pcre_compile(pattern, 0, &error, &erroffset, NULL);
    if (!re) {
        // Return error
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Regex error at offset %d: %s", erroffset, error);
        return value_make_string(error_msg);
    }
    
    pcre_extra* extra = pcre_study(re, 0, &error);
    
    Regex* regex = ALLOC(Regex);
    regex->pattern = re;
    regex->extra = extra;
    regex->source = str_copy(pattern);
    
    // Would return as Regex object
    Value val;
    val.type = VAL_OBJECT;
    // val.as.object = (Object*)regex;
    return val;
}

Value swiftflow_regex_match(Value regex_val, const char* subject) {
    // Assuming regex_val contains a Regex object
    // This is simplified
    
    // For now, return boolean
    // In real implementation, would use pcre_exec
    return value_make_bool(true);
}

Value swiftflow_regex_find_all(const char* pattern, const char* subject) {
    const char* error;
    int erroffset;
    int ovector[30]; // Output vector for matches
    
    pcre* re = pcre_compile(pattern, 0, &error, &erroffset, NULL);
    if (!re) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Regex error at offset %d: %s", erroffset, error);
        return value_make_string(error_msg);
    }
    
    pcre_extra* extra = pcre_study(re, 0, &error);
    
    // Create array for results
    Value array_val;
    array_val.type = VAL_ARRAY;
    array_val.as.array.count = 0;
    array_val.as.array.capacity = 10;
    array_val.as.array.elements = ALLOC_ARRAY(Value, 10);
    
    const char* start = subject;
    int subject_len = strlen(subject);
    int offset = 0;
    
    while (offset < subject_len) {
        int rc = pcre_exec(re, extra, start, subject_len, offset, 0, ovector, 30);
        
        if (rc < 0) {
            break; // No more matches
        }
        
        // Extract match
        int match_start = ovector[0];
        int match_end = ovector[1];
        int match_len = match_end - match_start;
        
        if (match_len > 0) {
            char* match_str = ALLOC_ARRAY(char, match_len + 1);
            strncpy(match_str, start + match_start, match_len);
            match_str[match_len] = '\0';
            
            // Add to array
            if (array_val.as.array.count >= array_val.as.array.capacity) {
                array_val.as.array.capacity *= 2;
                array_val.as.array.elements = REALLOC(array_val.as.array.elements, Value, 
                                                     array_val.as.array.capacity);
            }
            
            array_val.as.array.elements[array_val.as.array.count] = value_make_string(match_str);
            array_val.as.array.count++;
            
            free(match_str);
        }
        
        offset = match_end;
        if (match_start == match_end) {
            offset++; // Avoid infinite loop on zero-length matches
        }
    }
    
    pcre_free(re);
    if (extra) pcre_free_study(extra);
    
    return array_val;
}

char* swiftflow_regex_replace(const char* pattern, const char* replacement, const char* subject) {
    // Simplified version - in reality would use pcre_substitute
    // For now, return the subject unchanged
    return str_copy(subject);
}

// Register regex functions in interpreter
void regexlib_register(SwiftFlowInterpreter* interpreter) {
    // Register built-in regex functions
}

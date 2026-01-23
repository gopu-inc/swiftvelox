#include "jsonlib.h"
#include "interpreter.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

// ======================================================
// [SECTION] FONCTIONS AUXILIAIRES
// ======================================================

static char* json_value_to_string(Value value) {
    switch (value.type) {
        case VAL_STRING:
            return str_copy(value.as.str_val);
        case VAL_INT:
            return str_format("%ld", value.as.int_val);
        case VAL_FLOAT: {
            return str_format("%g", value.as.float_val);
        }
        case VAL_BOOL:
            return str_copy(value.as.bool_val ? "true" : "false");
        case VAL_NULL:
            return str_copy("null");
        case VAL_UNDEFINED:
            return str_copy("undefined");
        case VAL_ARRAY:
            return str_format("[array:%d]", value.as.array.count);
        case VAL_MAP:
            return str_format("{map:%d}", value.as.map.count);
        default:
            return str_copy("");
    }
}

// ======================================================
// [SECTION] JSON PARSER
// ======================================================

typedef struct {
    const char* start;
    const char* current;
    const char* source;
    int line;
    int column;
} JsonParser;

static char json_advance(JsonParser* parser) {
    parser->current++;
    parser->column++;
    return parser->current[-1];
}

static char json_peek(JsonParser* parser) {
    return *parser->current;
}

static bool json_is_at_end(JsonParser* parser) {
    return *parser->current == '\0';
}

static void json_skip_whitespace(JsonParser* parser) {
    while (true) {
        char c = json_peek(parser);
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                json_advance(parser);
                break;
            default:
                return;
        }
    }
}

static JsonValue* json_parse_value(JsonParser* parser);
static JsonValue* json_parse_array(JsonParser* parser);
static JsonValue* json_parse_object(JsonParser* parser);
static JsonValue* json_parse_string_literal(JsonParser* parser);
static JsonValue* json_parse_number(JsonParser* parser);
static JsonValue* json_parse_literal(JsonParser* parser);

// Fonction publique pour parser une chaîne JSON
JsonValue* json_parse_string(const char* str) {
    if (!str) return NULL;
    
    JsonParser parser;
    parser.start = str;
    parser.current = str;
    parser.source = str;
    parser.line = 1;
    parser.column = 1;
    
    JsonValue* result = json_parse_value(&parser);
    
    json_skip_whitespace(&parser);
    if (!json_is_at_end(&parser)) {
        // Invalid JSON - extra characters
        if (result) json_free(result);
        return NULL;
    }
    
    return result;
}

static JsonValue* json_parse_value(JsonParser* parser) {
    json_skip_whitespace(parser);
    
    if (json_is_at_end(parser)) {
        return NULL;
    }
    
    char c = json_peek(parser);
    
    if (c == '"') {
        return json_parse_string_literal(parser);
    } else if (c == '[') {
        return json_parse_array(parser);
    } else if (c == '{') {
        return json_parse_object(parser);
    } else if (isdigit(c) || c == '-') {
        return json_parse_number(parser);
    } else {
        return json_parse_literal(parser);
    }
}

static JsonValue* json_parse_array(JsonParser* parser) {
    JsonValue* array = ALLOC(JsonValue);
    if (!array) return NULL;
    
    array->type = JSON_ARRAY;
    array->as.array.elements = NULL;
    array->as.array.count = 0;
    array->as.array.capacity = 0;
    
    // Consume '['
    json_advance(parser);
    json_skip_whitespace(parser);
    
    // Check for empty array
    if (json_peek(parser) == ']') {
        json_advance(parser);
        return array;
    }
    
    // Parse elements
    while (true) {
        json_skip_whitespace(parser);
        
        JsonValue* element = json_parse_value(parser);
        if (!element) {
            json_free(array);
            return NULL;
        }
        
        // Add element to array
        if (array->as.array.count >= array->as.array.capacity) {
            int new_capacity = array->as.array.capacity == 0 ? 4 : array->as.array.capacity * 2;
            JsonValue** new_elements = REALLOC(array->as.array.elements, JsonValue*, new_capacity);
            if (!new_elements) {
                json_free(element);
                json_free(array);
                return NULL;
            }
            array->as.array.elements = new_elements;
            array->as.array.capacity = new_capacity;
        }
        
        array->as.array.elements[array->as.array.count] = element;
        array->as.array.count++;
        
        json_skip_whitespace(parser);
        
        // Check for next element or end
        if (json_peek(parser) == ',') {
            json_advance(parser);
        } else if (json_peek(parser) == ']') {
            json_advance(parser);
            break;
        } else {
            // Syntax error
            json_free(array);
            return NULL;
        }
    }
    
    return array;
}

static JsonValue* json_parse_object(JsonParser* parser) {
    JsonValue* object = ALLOC(JsonValue);
    if (!object) return NULL;
    
    object->type = JSON_OBJECT;
    object->as.object.keys = NULL;
    object->as.object.values = NULL;
    object->as.object.count = 0;
    object->as.object.capacity = 0;
    
    // Consume '{'
    json_advance(parser);
    json_skip_whitespace(parser);
    
    // Check for empty object
    if (json_peek(parser) == '}') {
        json_advance(parser);
        return object;
    }
    
    // Parse key-value pairs
    while (true) {
        json_skip_whitespace(parser);
        
        // Parse key
        if (json_peek(parser) != '"') {
            json_free(object);
            return NULL;
        }
        
        JsonValue* key_json = json_parse_string_literal(parser);
        if (!key_json || key_json->type != JSON_STRING) {
            if (key_json) json_free(key_json);
            json_free(object);
            return NULL;
        }
        
        char* key = key_json->as.str_val;
        free(key_json); // We just need the string
        
        json_skip_whitespace(parser);
        
        // Check for ':'
        if (json_peek(parser) != ':') {
            free(key);
            json_free(object);
            return NULL;
        }
        json_advance(parser);
        
        // Parse value
        JsonValue* value = json_parse_value(parser);
        if (!value) {
            free(key);
            json_free(object);
            return NULL;
        }
        
        // Add to object
        if (object->as.object.count >= object->as.object.capacity) {
            int new_capacity = object->as.object.capacity == 0 ? 4 : object->as.object.capacity * 2;
            
            char** new_keys = REALLOC(object->as.object.keys, char*, new_capacity);
            JsonValue** new_values = REALLOC(object->as.object.values, JsonValue*, new_capacity);
            
            if (!new_keys || !new_values) {
                free(new_keys);
                free(new_values);
                free(key);
                json_free(value);
                json_free(object);
                return NULL;
            }
            
            object->as.object.keys = new_keys;
            object->as.object.values = new_values;
            object->as.object.capacity = new_capacity;
        }
        
        object->as.object.keys[object->as.object.count] = key;
        object->as.object.values[object->as.object.count] = value;
        object->as.object.count++;
        
        json_skip_whitespace(parser);
        
        // Check for next pair or end
        if (json_peek(parser) == ',') {
            json_advance(parser);
        } else if (json_peek(parser) == '}') {
            json_advance(parser);
            break;
        } else {
            json_free(object);
            return NULL;
        }
    }
    
    return object;
}

static JsonValue* json_parse_string_literal(JsonParser* parser) {
    // Skip opening quote
    json_advance(parser);
    
    const char* start = parser->current;
    int length = 0;
    
    while (json_peek(parser) != '"' && !json_is_at_end(parser)) {
        if (json_peek(parser) == '\\') {
            // Skip escape character
            json_advance(parser);
            if (json_is_at_end(parser)) {
                return NULL;
            }
        }
        json_advance(parser);
        length++;
    }
    
    if (json_is_at_end(parser)) {
        return NULL; // Unterminated string
    }
    
    // Skip closing quote
    json_advance(parser);
    
    // Create string
    JsonValue* string_val = ALLOC(JsonValue);
    if (!string_val) return NULL;
    
    string_val->type = JSON_STRING;
    string_val->as.str_val = str_ncopy(start, length);
    
    if (!string_val->as.str_val) {
        free(string_val);
        return NULL;
    }
    
    return string_val;
}

static JsonValue* json_parse_number(JsonParser* parser) {
    const char* start = parser->current;
    bool is_float = false;
    
    // Optional minus sign
    if (json_peek(parser) == '-') {
        json_advance(parser);
    }
    
    // Integer part
    while (isdigit(json_peek(parser))) {
        json_advance(parser);
    }
    
    // Fraction part
    if (json_peek(parser) == '.') {
        is_float = true;
        json_advance(parser);
        
        while (isdigit(json_peek(parser))) {
            json_advance(parser);
        }
    }
    
    // Exponent part
    if (json_peek(parser) == 'e' || json_peek(parser) == 'E') {
        is_float = true;
        json_advance(parser);
        
        if (json_peek(parser) == '+' || json_peek(parser) == '-') {
            json_advance(parser);
        }
        
        while (isdigit(json_peek(parser))) {
            json_advance(parser);
        }
    }
    
    // Parse the number
    int length = parser->current - start;
    if (length <= 0) {
        return NULL;
    }
    
    char* num_str = str_ncopy(start, length);
    if (!num_str) return NULL;
    
    JsonValue* number = ALLOC(JsonValue);
    if (!number) {
        free(num_str);
        return NULL;
    }
    
    if (is_float) {
        number->type = JSON_FLOAT;
        number->as.float_val = atof(num_str);
    } else {
        number->type = JSON_INT;
        number->as.int_val = strtoll(num_str, NULL, 10);
    }
    
    free(num_str);
    return number;
}

static JsonValue* json_parse_literal(JsonParser* parser) {
    JsonValue* literal = ALLOC(JsonValue);
    if (!literal) return NULL;
    
    if (strncmp(parser->current, "true", 4) == 0) {
        literal->type = JSON_BOOL;
        literal->as.bool_val = true;
        parser->current += 4;
    } else if (strncmp(parser->current, "false", 5) == 0) {
        literal->type = JSON_BOOL;
        literal->as.bool_val = false;
        parser->current += 5;
    } else if (strncmp(parser->current, "null", 4) == 0) {
        literal->type = JSON_NULL;
        parser->current += 4;
    } else {
        free(literal);
        return NULL;
    }
    
    return literal;
}

// ======================================================
// [SECTION] JSON STRINGIFY
// ======================================================

static void json_append_string(char** buffer, int* size, int* capacity, const char* str) {
    if (!str) return;
    
    int len = strlen(str);
    
    while (*size + len >= *capacity) {
        *capacity = *capacity == 0 ? 256 : *capacity * 2;
        char* new_buffer = REALLOC(*buffer, char, *capacity);
        if (!new_buffer) return;
        *buffer = new_buffer;
    }
    
    strcpy(*buffer + *size, str);
    *size += len;
}

static void json_stringify_value(JsonValue* json, char** buffer, int* size, int* capacity, int indent) {
    if (!json) {
        json_append_string(buffer, size, capacity, "null");
        return;
    }
    
    switch (json->type) {
        case JSON_NULL:
            json_append_string(buffer, size, capacity, "null");
            break;
            
        case JSON_BOOL:
            json_append_string(buffer, size, capacity, json->as.bool_val ? "true" : "false");
            break;
            
        case JSON_INT: {
            char num_str[32];
            snprintf(num_str, sizeof(num_str), "%lld", (long long)json->as.int_val);
            json_append_string(buffer, size, capacity, num_str);
            break;
        }
            
        case JSON_FLOAT: {
            char num_str[32];
            snprintf(num_str, sizeof(num_str), "%g", json->as.float_val);
            json_append_string(buffer, size, capacity, num_str);
            break;
        }
            
        case JSON_STRING: {
            json_append_string(buffer, size, capacity, "\"");
            if (json->as.str_val) {
                json_append_string(buffer, size, capacity, json->as.str_val);
            }
            json_append_string(buffer, size, capacity, "\"");
            break;
        }
            
        case JSON_ARRAY: {
            json_append_string(buffer, size, capacity, "[");
            
            for (int i = 0; i < json->as.array.count; i++) {
                if (i > 0) {
                    json_append_string(buffer, size, capacity, ",");
                }
                json_stringify_value(json->as.array.elements[i], buffer, size, capacity, indent);
            }
            
            json_append_string(buffer, size, capacity, "]");
            break;
        }
            
        case JSON_OBJECT: {
            json_append_string(buffer, size, capacity, "{");
            
            for (int i = 0; i < json->as.object.count; i++) {
                if (i > 0) {
                    json_append_string(buffer, size, capacity, ",");
                }
                
                // Key
                json_append_string(buffer, size, capacity, "\"");
                if (json->as.object.keys[i]) {
                    json_append_string(buffer, size, capacity, json->as.object.keys[i]);
                }
                json_append_string(buffer, size, capacity, "\":");
                
                // Value
                json_stringify_value(json->as.object.values[i], buffer, size, capacity, indent);
            }
            
            json_append_string(buffer, size, capacity, "}");
            break;
        }
    }
}

char* json_stringify(JsonValue* json) {
    char* buffer = NULL;
    int size = 0;
    int capacity = 0;
    
    json_stringify_value(json, &buffer, &size, &capacity, 0);
    
    if (buffer && size > 0) {
        buffer[size] = '\0';
        char* resized = REALLOC(buffer, char, size + 1);
        if (resized) {
            buffer = resized;
        }
    } else if (buffer) {
        free(buffer);
        buffer = str_copy("null");
    }
    
    return buffer;
}

// ======================================================
// [SECTION] MEMORY MANAGEMENT
// ======================================================

void json_free(JsonValue* json) {
    if (!json) return;
    
    switch (json->type) {
        case JSON_STRING:
            FREE(json->as.str_val);
            break;
            
        case JSON_ARRAY:
            if (json->as.array.elements) {
                for (int i = 0; i < json->as.array.count; i++) {
                    json_free(json->as.array.elements[i]);
                }
                FREE(json->as.array.elements);
            }
            break;
            
        case JSON_OBJECT:
            if (json->as.object.keys && json->as.object.values) {
                for (int i = 0; i < json->as.object.count; i++) {
                    FREE(json->as.object.keys[i]);
                    json_free(json->as.object.values[i]);
                }
                FREE(json->as.object.keys);
                FREE(json->as.object.values);
            }
            break;
            
        default:
            break;
    }
    
    FREE(json);
}

// ======================================================
// [SECTION] SWIFTFLOW-JSON CONVERSION
// ======================================================

JsonValue* swiftflow_value_to_json(Value value) {
    JsonValue* json = ALLOC(JsonValue);
    if (!json) return NULL;
    
    switch (value.type) {
        case VAL_NULL:
            json->type = JSON_NULL;
            break;
            
        case VAL_BOOL:
            json->type = JSON_BOOL;
            json->as.bool_val = value.as.bool_val;
            break;
            
        case VAL_INT:
            json->type = JSON_INT;
            json->as.int_val = value.as.int_val;
            break;
            
        case VAL_FLOAT:
            json->type = JSON_FLOAT;
            json->as.float_val = value.as.float_val;
            break;
            
        case VAL_STRING:
            json->type = JSON_STRING;
            json->as.str_val = str_copy(value.as.str_val);
            if (!json->as.str_val) {
                free(json);
                return NULL;
            }
            break;
            
        case VAL_ARRAY: {
            json->type = JSON_ARRAY;
            json->as.array.count = value.as.array.count;
            json->as.array.capacity = value.as.array.capacity;
            
            if (value.as.array.count > 0) {
                json->as.array.elements = ALLOC_ARRAY(JsonValue*, value.as.array.count);
                if (!json->as.array.elements) {
                    free(json);
                    return NULL;
                }
                
                for (int i = 0; i < value.as.array.count; i++) {
                    json->as.array.elements[i] = swiftflow_value_to_json(value.as.array.elements[i]);
                    if (!json->as.array.elements[i]) {
                        // Cleanup on error
                        for (int j = 0; j < i; j++) {
                            json_free(json->as.array.elements[j]);
                        }
                        free(json->as.array.elements);
                        free(json);
                        return NULL;
                    }
                }
            } else {
                json->as.array.elements = NULL;
            }
            break;
        }
            
        case VAL_MAP: {
            json->type = JSON_OBJECT;
            json->as.object.count = value.as.map.count;
            json->as.object.capacity = value.as.map.capacity;
            
            if (value.as.map.count > 0) {
                json->as.object.keys = ALLOC_ARRAY(char*, value.as.map.count);
                json->as.object.values = ALLOC_ARRAY(JsonValue*, value.as.map.count);
                
                if (!json->as.object.keys || !json->as.object.values) {
                    free(json->as.object.keys);
                    free(json->as.object.values);
                    free(json);
                    return NULL;
                }
                
                for (int i = 0; i < value.as.map.count; i++) {
                    json->as.object.keys[i] = str_copy(value.as.map.keys[i]);
                    json->as.object.values[i] = swiftflow_value_to_json(value.as.map.values[i]);
                    
                    if (!json->as.object.keys[i] || !json->as.object.values[i]) {
                        // Cleanup on error
                        for (int j = 0; j < i; j++) {
                            free(json->as.object.keys[j]);
                            json_free(json->as.object.values[j]);
                        }
                        free(json->as.object.keys);
                        free(json->as.object.values);
                        free(json);
                        return NULL;
                    }
                }
            } else {
                json->as.object.keys = NULL;
                json->as.object.values = NULL;
            }
            break;
        }
            
        default:
            // Default to null for unsupported types
            json->type = JSON_NULL;
            break;
    }
    
    return json;
}

Value json_to_swiftflow_value(JsonValue* json) {
    if (!json) return value_make_null();
    
    switch (json->type) {
        case JSON_NULL:
            return value_make_null();
            
        case JSON_BOOL:
            return value_make_bool(json->as.bool_val);
            
        case JSON_INT:
            return value_make_int(json->as.int_val);
            
        case JSON_FLOAT:
            return value_make_float(json->as.float_val);
            
        case JSON_STRING:
            return value_make_string(json->as.str_val);
            
        case JSON_ARRAY: {
            Value array_val;
            array_val.type = VAL_ARRAY;
            array_val.as.array.count = json->as.array.count;
            array_val.as.array.capacity = json->as.array.capacity;
            
            if (json->as.array.count > 0) {
                array_val.as.array.elements = ALLOC_ARRAY(Value, json->as.array.count);
                if (!array_val.as.array.elements) {
                    return value_make_null();
                }
                
                for (int i = 0; i < json->as.array.count; i++) {
                    array_val.as.array.elements[i] = json_to_swiftflow_value(json->as.array.elements[i]);
                }
            } else {
                array_val.as.array.elements = NULL;
            }
            return array_val;
        }
            
        case JSON_OBJECT: {
            Value map_val;
            map_val.type = VAL_MAP;
            map_val.as.map.count = json->as.object.count;
            map_val.as.map.capacity = json->as.object.capacity;
            
            if (json->as.object.count > 0) {
                map_val.as.map.keys = ALLOC_ARRAY(char*, json->as.object.count);
                map_val.as.map.values = ALLOC_ARRAY(Value, json->as.object.count);
                
                if (!map_val.as.map.keys || !map_val.as.map.values) {
                    free(map_val.as.map.keys);
                    free(map_val.as.map.values);
                    return value_make_null();
                }
                
                for (int i = 0; i < json->as.object.count; i++) {
                    map_val.as.map.keys[i] = str_copy(json->as.object.keys[i]);
                    map_val.as.map.values[i] = json_to_swiftflow_value(json->as.object.values[i]);
                    
                    if (!map_val.as.map.keys[i]) {
                        // Cleanup on error
                        for (int j = 0; j < i; j++) {
                            free(map_val.as.map.keys[j]);
                            value_free(&map_val.as.map.values[j]);
                        }
                        free(map_val.as.map.keys);
                        free(map_val.as.map.values);
                        return value_make_null();
                    }
                }
            } else {
                map_val.as.map.keys = NULL;
                map_val.as.map.values = NULL;
            }
            return map_val;
        }
    }
    
    return value_make_null();
}

// ======================================================
// [SECTION] PUBLIC API
// ======================================================

Value swiftflow_json_parse(const char* json_str) {
    if (!json_str) {
        return value_make_string("Invalid JSON: null string");
    }
    
    JsonValue* json = json_parse_string(json_str);
    if (!json) {
        return value_make_string("Invalid JSON: parse error");
    }
    
    Value result = json_to_swiftflow_value(json);
    json_free(json);
    return result;
}

char* swiftflow_json_stringify(Value value) {
    JsonValue* json = swiftflow_value_to_json(value);
    if (!json) return NULL;
    
    char* result = json_stringify(json);
    json_free(json);
    return result;
}

Value swiftflow_json_read_file(const char* filename) {
    if (!filename) {
        return value_make_string("Invalid filename");
    }
    
    FILE* file = fopen(filename, "r");
    if (!file) {
        return value_make_string("Cannot open file");
    }
    
    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    rewind(file);
    
    if (size <= 0) {
        fclose(file);
        return value_make_string("Empty file");
    }
    
    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return value_make_string("Memory allocation failed");
    }
    
    size_t read = fread(buffer, 1, size, file);
    buffer[read] = '\0';
    fclose(file);
    
    JsonValue* json = json_parse_string(buffer);
    free(buffer);
    
    if (!json) {
        return value_make_string("Invalid JSON in file");
    }
    
    Value result = json_to_swiftflow_value(json);
    json_free(json);
    return result;
}

bool swiftflow_json_write_file(const char* filename, Value value) {
    if (!filename) return false;
    
    JsonValue* json = swiftflow_value_to_json(value);
    if (!json) return false;
    
    char* json_str = json_stringify(json);
    json_free(json);
    
    if (!json_str) return false;
    
    FILE* file = fopen(filename, "w");
    if (!file) {
        free(json_str);
        return false;
    }
    
    fprintf(file, "%s", json_str);
    fclose(file);
    free(json_str);
    return true;
}

// ======================================================
// [SECTION] FONCTIONS JSON POUR SWIFTFLOW (NON-STATIC)
// ======================================================

Value json_builtin_parse(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 1) {
        interpreter_error(interpreter, "json.parse() expects 1 argument", 0, 0);
        return value_make_undefined();
    }
    
    char* json_str = json_value_to_string(args[0]);
    if (!json_str) {
        interpreter_error(interpreter, "Failed to convert to string", 0, 0);
        return value_make_undefined();
    }
    
    Value result = swiftflow_json_parse(json_str);
    free(json_str);
    
    return result;
}

Value json_builtin_stringify(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 1) {
        interpreter_error(interpreter, "json.stringify() expects 1 argument", 0, 0);
        return value_make_undefined();
    }
    
    char* json_str = swiftflow_json_stringify(args[0]);
    if (!json_str) {
        return value_make_string("{}");
    }
    
    Value result = value_make_string(json_str);
    free(json_str);
    
    return result;
}

Value json_builtin_read_file(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 1) {
        interpreter_error(interpreter, "json.read_file() expects 1 argument", 0, 0);
        return value_make_undefined();
    }
    
    if (args[0].type != VAL_STRING) {
        interpreter_error(interpreter, "json.read_file() expects string filename", 0, 0);
        return value_make_undefined();
    }
    
    return swiftflow_json_read_file(args[0].as.str_val);
}

Value json_builtin_write_file(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 2) {
        interpreter_error(interpreter, "json.write_file() expects 2 arguments", 0, 0);
        return value_make_undefined();
    }
    
    if (args[0].type != VAL_STRING) {
        interpreter_error(interpreter, "json.write_file() first argument must be filename string", 0, 0);
        return value_make_undefined();
    }
    
    bool success = swiftflow_json_write_file(args[0].as.str_val, args[1]);
    return value_make_bool(success);
}

// ======================================================
// [SECTION] PACKAGE REGISTRATION
// ======================================================

void jsonlib_register(SwiftFlowInterpreter* interpreter) {
    // Cette fonction est appelée depuis interpreter.c
    (void)interpreter; // Mark as used to avoid warning
}

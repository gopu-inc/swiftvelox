#include "common.h"
#include <jsonlib.h>
#include "interpreter.h"
#include <stdio.h>
#include <ctype.h>
#include <string.h>

// SwiftFlow JSON library using Jansson

Value swiftflow_json_parse(const char* json_str) {
    json_error_t error;
    json_t* root = json_loads(json_str, 0, &error);
    
    if (!root) {
        // Return error string
        return value_make_string(error.text);
    }
    
    return json_to_swiftflow_value(root);
}

Value json_to_swiftflow_value(json_t* json) {
    if (!json) return value_make_null();
    
    switch (json_typeof(json)) {
        case JSON_NULL:
            return value_make_null();
            
        case JSON_TRUE:
            return value_make_bool(true);
            
        case JSON_FALSE:
            return value_make_bool(false);
            
        case JSON_INTEGER: {
            json_int_t val = json_integer_value(json_t);
            return value_make_int((int64)val);
        }
            
        case JSON_REAL:
            return value_make_float(json_real_value(json));
            
        case JSON_STRING:
            return value_make_string(json_string_value(json));
            
        case JSON_ARRAY: {
            Value array_val;
            array_val.type = VAL_ARRAY;
            array_val.as.array.count = json_array_size(json);
            array_val.as.array.capacity = array_val.as.array.count;
            array_val.as.array.elements = ALLOC_ARRAY(Value, array_val.as.array.count);
            
            for (size_t i = 0; i < array_val.as.array.count; i++) {
                json_t* elem = json_array_get(json, i);
                array_val.as.array.elements[i] = json_to_swiftflow_value(elem);
            }
            
            return array_val;
        }
            
        case JSON_OBJECT: {
            Value map_val;
            map_val.type = VAL_MAP;
            map_val.as.map.count = json_object_size(json);
            map_val.as.map.capacity = map_val.as.map.count;
            map_val.as.map.keys = ALLOC_ARRAY(char*, map_val.as.map.count);
            map_val.as.map.values = ALLOC_ARRAY(Value, map_val.as.map.count);
            
            size_t idx = 0;
            const char* key;
            json_t* value;
            json_object_foreach(json, key, value) {
                map_val.as.map.keys[idx] = str_copy(key);
                map_val.as.map.values[idx] = json_to_swiftflow_value(value);
                idx++;
            }
            
            return map_val;
        }
    }
    
    return value_make_undefined();
}

json_t* swiftflow_value_to_json(Value value) {
    switch (value.type) {
        case VAL_NULL:
            return json_null();
            
        case VAL_BOOL:
            return json_boolean(value.as.bool_val);
            
        case VAL_INT:
            return json_integer(value.as.int_val);
            
        case VAL_FLOAT:
            return json_real(value.as.float_val);
            
        case VAL_STRING:
            return json_string(value.as.str_val);
            
        case VAL_ARRAY: {
            json_t* array = json_array();
            for (int i = 0; i < value.as.array.count; i++) {
                json_array_append_new(array, swiftflow_value_to_json(value.as.array.elements[i]));
            }
            return array;
        }
            
        case VAL_MAP: {
            json_t* object = json_object();
            for (int i = 0; i < value.as.map.count; i++) {
                json_object_set_new(object, value.as.map.keys[i], 
                                   swiftflow_value_to_json(value.as.map.values[i]));
            }
            return object;
        }
            
        default:
            return json_null();
    }
}

char* swiftflow_json_stringify(Value value) {
    json_t* json = swiftflow_value_to_json(value);
    char* result = json_dumps(json, JSON_INDENT(2) | JSON_ENSURE_ASCII);
    json_decref(json);
    return result;
}

Value swiftflow_json_read_file(const char* filename) {
    json_error_t error;
    json_t* root = json_load_file(filename, 0, &error);
    
    if (!root) {
        return value_make_string(error.text);
    }
    
    Value result = json_to_swiftflow_value(root);
    json_decref(root);
    return result;
}

bool swiftflow_json_write_file(const char* filename, Value value) {
    json_t* json = swiftflow_value_to_json(value);
    int result = json_dump_file(json, filename, JSON_INDENT(2) | JSON_ENSURE_ASCII);
    json_decref(json);
    return result == 0;
}

// Register JSON functions in interpreter
void jsonlib_register(SwiftFlowInterpreter* interpreter) {
    // Register built-in JSON functions
}

#include "value.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdio.h>

// Helper pour strdup
#ifndef strdup
char* my_strdup(const char* s) {
    if (!s) return NULL;
    size_t len = strlen(s) + 1;
    char* copy = malloc(len);
    if (copy) memcpy(copy, s, len);
    return copy;
}
#define strdup my_strdup
#endif

Value make_number(double value) {
    Value v;
    if (floor(value) == value && value <= 9223372036854775807.0 && value >= -9223372036854775808.0) {
        v.type = VAL_INT;
        v.integer = (int64_t)value;
    } else {
        v.type = VAL_FLOAT;
        v.number = value;
    }
    return v;
}

Value make_string(const char* str) {
    Value v;
    v.type = VAL_STRING;
    v.string = strdup(str ? str : "");
    return v;
}

Value make_bool(int b) {
    Value v;
    v.type = VAL_BOOL;
    v.boolean = b ? 1 : 0;
    return v;
}

Value make_nil() {
    Value v;
    v.type = VAL_NIL;
    return v;
}

Value make_array() {
    Value v;
    v.type = VAL_ARRAY;
    v.array.items = NULL;
    v.array.count = 0;
    v.array.capacity = 0;
    return v;
}

Value make_object() {
    Value v;
    v.type = VAL_OBJECT;
    v.object.keys = NULL;
    v.object.values = NULL;
    v.object.count = 0;
    v.object.capacity = 0;
    return v;
}

Value make_error(const char* message) {
    Value v;
    v.type = VAL_ERROR;
    v.error.message = strdup(message ? message : "Unknown error");
    v.error.data = NULL;
    return v;
}

void array_push(Value* array, Value item) {
    if (array->type != VAL_ARRAY) return;
    
    if (array->array.count >= array->array.capacity) {
        int new_capacity = array->array.capacity == 0 ? 8 : array->array.capacity * 2;
        Value* new_items = realloc(array->array.items, sizeof(Value) * new_capacity);
        if (!new_items) return;
        array->array.items = new_items;
        array->array.capacity = new_capacity;
    }
    
    array->array.items[array->array.count++] = item;
}

void object_set(Value* obj, const char* key, Value value) {
    if (obj->type != VAL_OBJECT) return;
    
    for (int i = 0; i < obj->object.count; i++) {
        if (strcmp(obj->object.keys[i], key) == 0) {
            obj->object.values[i] = value;
            return;
        }
    }
    
    if (obj->object.count >= obj->object.capacity) {
        int new_capacity = obj->object.capacity == 0 ? 8 : obj->object.capacity * 2;
        char** new_keys = realloc(obj->object.keys, sizeof(char*) * new_capacity);
        Value* new_values = realloc(obj->object.values, sizeof(Value) * new_capacity);
        if (!new_keys || !new_values) return;
        obj->object.keys = new_keys;
        obj->object.values = new_values;
        obj->object.capacity = new_capacity;
    }
    
    obj->object.keys[obj->object.count] = strdup(key ? key : "");
    obj->object.values[obj->object.count] = value;
    obj->object.count++;
}

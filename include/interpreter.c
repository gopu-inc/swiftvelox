#include "interpreter.h"
#include "common.h"
#include <math.h>
#include <ctype.h>

// ======================================================
// [SECTION] VALUE OPERATIONS
// ======================================================

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
            return str_format("%ld",

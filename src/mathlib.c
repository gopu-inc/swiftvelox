#include "common.h"
#include <tommath.h>

// SwiftFlow math library using LibTomMath

typedef struct {
    mp_int value;
} BigInt;

Value swiftflow_math_add(Value a, Value b) {
    if (a.type == VAL_INT && b.type == VAL_INT) {
        // Use big integers for large numbers
        if (a.as.int_val > INT64_MAX / 2 || b.as.int_val > INT64_MAX / 2) {
            // Use LibTomMath
            mp_int ma, mb, result;
            mp_init(&ma);
            mp_init(&mb);
            mp_init(&result);
            
            mp_set_i64(&ma, a.as.int_val);
            mp_set_i64(&mb, b.as.int_val);
            mp_add(&ma, &mb, &result);
            
            // Convert back to int64 if possible
            if (mp_cmp_d(&result, INT64_MAX) == MP_LT) {
                int64_t val = mp_get_i64(&result);
                mp_clear_multi(&ma, &mb, &result, NULL);
                return value_make_int(val);
            } else {
                // Store as BigInt object
                BigInt* big = ALLOC(BigInt);
                mp_init_copy(&big->value, &result);
                mp_clear_multi(&ma, &mb, &result, NULL);
                
                Value val;
                val.type = VAL_OBJECT;
                val.as.object = NULL; // Would be pointer to BigInt object
                return val;
            }
        } else {
            // Use normal integers
            return value_make_int(a.as.int_val + b.as.int_val);
        }
    }
    
    // Fallback to float
    double da = (a.type == VAL_INT) ? (double)a.as.int_val : a.as.float_val;
    double db = (b.type == VAL_INT) ? (double)b.as.int_val : b.as.float_val;
    return value_make_float(da + db);
}

Value swiftflow_math_subtract(Value a, Value b) {
    if (a.type == VAL_INT && b.type == VAL_INT) {
        return value_make_int(a.as.int_val - b.as.int_val);
    }
    
    double da = (a.type == VAL_INT) ? (double)a.as.int_val : a.as.float_val;
    double db = (b.type == VAL_INT) ? (double)b.as.int_val : b.as.float_val;
    return value_make_float(da - db);
}

Value swiftflow_math_multiply(Value a, Value b) {
    if (a.type == VAL_INT && b.type == VAL_INT) {
        // Check for overflow
        if (a.as.int_val != 0 && b.as.int_val > INT64_MAX / a.as.int_val) {
            // Use LibTomMath
            mp_int ma, mb, result;
            mp_init(&ma);
            mp_init(&mb);
            mp_init(&result);
            
            mp_set_i64(&ma, a.as.int_val);
            mp_set_i64(&mb, b.as.int_val);
            mp_mul(&ma, &mb, &result);
            
            // Try to convert back
            if (mp_cmp_d(&result, INT64_MAX) == MP_LT) {
                int64_t val = mp_get_i64(&result);
                mp_clear_multi(&ma, &mb, &result, NULL);
                return value_make_int(val);
            }
            
            mp_clear_multi(&ma, &mb, &result, NULL);
            // Return as float for very large numbers
            return value_make_float((double)a.as.int_val * (double)b.as.int_val);
        }
        return value_make_int(a.as.int_val * b.as.int_val);
    }
    
    double da = (a.type == VAL_INT) ? (double)a.as.int_val : a.as.float_val;
    double db = (b.type == VAL_INT) ? (double)b.as.int_val : b.as.float_val;
    return value_make_float(da * db);
}

Value swiftflow_math_divide(Value a, Value b) {
    if (a.type == VAL_INT && b.type == VAL_INT) {
        if (b.as.int_val == 0) {
            return value_make_undefined(); // Division by zero
        }
        
        if (a.as.int_val % b.as.int_val == 0) {
            return value_make_int(a.as.int_val / b.as.int_val);
        } else {
            return value_make_float((double)a.as.int_val / (double)b.as.int_val);
        }
    }
    
    double da = (a.type == VAL_INT) ? (double)a.as.int_val : a.as.float_val;
    double db = (b.type == VAL_INT) ? (double)b.as.int_val : b.as.float_val;
    
    if (db == 0.0) {
        return value_make_undefined(); // Division by zero
    }
    
    return value_make_float(da / db);
}

Value swiftflow_math_power(Value base, Value exponent) {
    double b = (base.type == VAL_INT) ? (double)base.as.int_val : base.as.float_val;
    double e = (exponent.type == VAL_INT) ? (double)exponent.as.int_val : exponent.as.float_val;
    
    return value_make_float(pow(b, e));
}

Value swiftflow_math_sqrt(Value x) {
    double val = (x.type == VAL_INT) ? (double)x.as.int_val : x.as.float_val;
    
    if (val < 0) {
        return value_make_nan();
    }
    
    return value_make_float(sqrt(val));
}

Value swiftflow_math_abs(Value x) {
    if (x.type == VAL_INT) {
        int64_t val = x.as.int_val;
        return value_make_int(val < 0 ? -val : val);
    } else if (x.type == VAL_FLOAT) {
        double val = x.as.float_val;
        return value_make_float(val < 0 ? -val : val);
    }
    
    return value_make_undefined();
}

Value swiftflow_math_round(Value x) {
    if (x.type == VAL_FLOAT) {
        return value_make_int((int64_t)round(x.as.float_val));
    }
    return x; // Integers are already rounded
}

Value swiftflow_math_floor(Value x) {
    if (x.type == VAL_FLOAT) {
        return value_make_int((int64_t)floor(x.as.float_val));
    }
    return x;
}

Value swiftflow_math_ceil(Value x) {
    if (x.type == VAL_FLOAT) {
        return value_make_int((int64_t)ceil(x.as.float_val));
    }
    return x;
}

// Register math functions in interpreter
void mathlib_register(SwiftFlowInterpreter* interpreter) {
    // These would be registered as built-in functions
    // For example: interpreter_register_function(interpreter, "sqrt", swiftflow_math_sqrt);
}

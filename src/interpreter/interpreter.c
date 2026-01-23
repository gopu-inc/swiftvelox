#include "interpreter.h"
#include "common.h"
#include <math.h>
#include <ctype.h>
#include <string.h>
#include <time.h>

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

Value value_make_nan(void) {
    Value value;
    value.type = VAL_NAN;
    value.as.float_val = NAN;
    return value;
}

Value value_make_inf(void) {
    Value value;
    value.type = VAL_INF;
    value.as.float_val = INFINITY;
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
        case VAL_FUNCTION:
            if (value->as.function) {
                FREE(value->as.function);
            }
            break;
        case VAL_OBJECT:
            if (value->as.object) {
                FREE(value->as.object);
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
            if (isnan(value.as.float_val)) {
                return str_copy("nan");
            }
            if (isinf(value.as.float_val)) {
                return str_copy(value.as.float_val > 0 ? "inf" : "-inf");
            }
            return str_format("%g", value.as.float_val);
        case VAL_BOOL:
            return str_copy(value.as.bool_val ? "true" : "false");
        case VAL_STRING:
            return str_format("\"%s\"", value.as.str_val);
        case VAL_NULL:
            return str_copy("null");
        case VAL_UNDEFINED:
            return str_copy("undefined");
        case VAL_NAN:
            return str_copy("nan");
        case VAL_INF:
            return str_copy("inf");
        case VAL_ARRAY:
            return str_format("[array:%d]", value.as.array.count);
        case VAL_MAP:
            return str_format("{map:%d}", value.as.map.count);
        case VAL_FUNCTION:
            return str_format("<function>");
        case VAL_OBJECT:
            return str_format("<object>");
        default:
            return str_format("<value:%d>", value.type);
    }
}

void value_print(Value value) {
    char* str = value_to_string(value);
    if (str) {
        printf("%s", str);
        free(str);
    }
}

bool value_is_truthy(Value value) {
    switch (value.type) {
        case VAL_BOOL:
            return value.as.bool_val;
        case VAL_NULL:
        case VAL_UNDEFINED:
        case VAL_NAN:
            return false;
        case VAL_INT:
            return value.as.int_val != 0;
        case VAL_FLOAT:
            return value.as.float_val != 0.0 && !isnan(value.as.float_val);
        case VAL_STRING:
            return value.as.str_val[0] != '\0';
        case VAL_ARRAY:
            return value.as.array.count > 0;
        case VAL_MAP:
            return value.as.map.count > 0;
        default:
            return true;
    }
}

bool value_equal(Value a, Value b) {
    if (a.type != b.type) {
        // Try type coercion
        if (a.type == VAL_INT && b.type == VAL_FLOAT) {
            return (double)a.as.int_val == b.as.float_val;
        }
        if (a.type == VAL_FLOAT && b.type == VAL_INT) {
            return a.as.float_val == (double)b.as.int_val;
        }
        return false;
    }
    
    switch (a.type) {
        case VAL_INT:
            return a.as.int_val == b.as.int_val;
        case VAL_FLOAT:
            return a.as.float_val == b.as.float_val;
        case VAL_BOOL:
            return a.as.bool_val == b.as.bool_val;
        case VAL_STRING:
            return str_equal(a.as.str_val, b.as.str_val);
        case VAL_NULL:
        case VAL_UNDEFINED:
            return true;
        case VAL_NAN:
            return false; // NaN != NaN
        case VAL_INF:
            return true; // Inf == Inf
        default:
            return false;
    }
}

// ======================================================
// [SECTION] ENVIRONMENT MANAGEMENT
// ======================================================

Environment* environment_new(Environment* parent) {
    Environment* env = ALLOC(Environment);
    CHECK_ALLOC(env, "Environment allocation");
    
    env->parent = parent;
    env->variables.names = NULL;
    env->variables.values = NULL;
    env->variables.count = 0;
    env->variables.capacity = 0;
    
    return env;
}

void environment_free(Environment* env) {
    if (!env) return;
    
    if (env->variables.names) {
        for (int i = 0; i < env->variables.count; i++) {
            FREE(env->variables.names[i]);
            value_free(&env->variables.values[i]);
        }
        FREE(env->variables.names);
        FREE(env->variables.values);
    }
    
    FREE(env);
}

void environment_define(Environment* env, const char* name, Value value) {
    // Check if variable already exists
    for (int i = 0; i < env->variables.count; i++) {
        if (str_equal(env->variables.names[i], name)) {
            // Update existing variable
            value_free(&env->variables.values[i]);
            env->variables.values[i] = value;
            return;
        }
    }
    
    // Add new variable
    if (env->variables.count >= env->variables.capacity) {
        int new_capacity = env->variables.capacity == 0 ? 8 : env->variables.capacity * 2;
        env->variables.names = REALLOC(env->variables.names, char*, new_capacity);
        env->variables.values = REALLOC(env->variables.values, Value, new_capacity);
        CHECK_ALLOC(env->variables.names, "Environment names reallocation");
        CHECK_ALLOC(env->variables.values, "Environment values reallocation");
        env->variables.capacity = new_capacity;
    }
    
    env->variables.names[env->variables.count] = str_copy(name);
    env->variables.values[env->variables.count] = value;
    env->variables.count++;
}

bool environment_set(Environment* env, const char* name, Value value) {
    // Look in current environment
    for (int i = 0; i < env->variables.count; i++) {
        if (str_equal(env->variables.names[i], name)) {
            value_free(&env->variables.values[i]);
            env->variables.values[i] = value;
            return true;
        }
    }
    
    // Look in parent environments
    if (env->parent) {
        return environment_set(env->parent, name, value);
    }
    
    return false;
}

Value environment_get(Environment* env, const char* name) {
    // Look in current environment
    for (int i = 0; i < env->variables.count; i++) {
        if (str_equal(env->variables.names[i], name)) {
            return env->variables.values[i];
        }
    }
    
    // Look in parent environments
    if (env->parent) {
        return environment_get(env->parent, name);
    }
    
    // Variable not found
    return value_make_undefined();
}

bool environment_exists(Environment* env, const char* name) {
    // Look in current environment
    for (int i = 0; i < env->variables.count; i++) {
        if (str_equal(env->variables.names[i], name)) {
            return true;
        }
    }
    
    // Look in parent environments
    if (env->parent) {
        return environment_exists(env->parent, name);
    }
    
    return false;
}

// ======================================================
// [SECTION] BUILT-IN FUNCTIONS
// ======================================================

typedef Value (*BuiltinFunction)(SwiftFlowInterpreter* interpreter, Value* args, int arg_count);

typedef struct {
    char* name;
    BuiltinFunction func;
    int min_args;
    int max_args;
} Builtin;

// Built-in print function
Value builtin_print(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    (void)interpreter; // Marquer comme utilisé
    for (int i = 0; i < arg_count; i++) {
        value_print(args[i]);
        if (i < arg_count - 1) {
            printf(" ");
        }
    }
    printf("\n");
    return value_make_null();
}

// Built-in input function
Value builtin_input(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    (void)interpreter; // Marquer comme utilisé
    if (arg_count > 0) {
        char* prompt = value_to_string(args[0]);
        printf("%s", prompt);
        fflush(stdout);
        free(prompt);
    }
    
    char buffer[1024];
    if (fgets(buffer, sizeof(buffer), stdin)) {
        // Remove trailing newline
        size_t len = strlen(buffer);
        if (len > 0 && buffer[len-1] == '\n') {
            buffer[len-1] = '\0';
        }
        return value_make_string(buffer);
    }
    return value_make_string("");
}

// Built-in length function
Value builtin_length(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 1) {
        interpreter_error(interpreter, "length() expects 1 argument", 0, 0);
        return value_make_undefined();
    }
    
    Value arg = args[0];
    switch (arg.type) {
        case VAL_STRING:
            return value_make_int(strlen(arg.as.str_val));
        case VAL_ARRAY:
            return value_make_int(arg.as.array.count);
        case VAL_MAP:
            return value_make_int(arg.as.map.count);
        default:
            interpreter_error(interpreter, "length() expects string, array, or map", 0, 0);
            return value_make_undefined();
    }
}

// Built-in type function
Value builtin_typeof(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 1) {
        interpreter_error(interpreter, "typeof() expects 1 argument", 0, 0);
        return value_make_undefined();
    }
    
    Value arg = args[0];
    const char* type_name;
    
    switch (arg.type) {
        case VAL_INT: type_name = "int"; break;
        case VAL_FLOAT: type_name = "float"; break;
        case VAL_BOOL: type_name = "bool"; break;
        case VAL_STRING: type_name = "string"; break;
        case VAL_NULL: type_name = "null"; break;
        case VAL_UNDEFINED: type_name = "undefined"; break;
        case VAL_NAN: type_name = "nan"; break;
        case VAL_INF: type_name = "inf"; break;
        case VAL_ARRAY: type_name = "array"; break;
        case VAL_MAP: type_name = "map"; break;
        case VAL_FUNCTION: type_name = "function"; break;
        case VAL_OBJECT: type_name = "object"; break;
        default: type_name = "unknown"; break;
    }
    
    return value_make_string(type_name);
}

// Built-in math functions
Value builtin_abs(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 1) {
        interpreter_error(interpreter, "abs() expects 1 argument", 0, 0);
        return value_make_undefined();
    }
    
    Value arg = args[0];
    if (arg.type == VAL_INT) {
        int64_t val = arg.as.int_val;
        return value_make_int(val < 0 ? -val : val);
    } else if (arg.type == VAL_FLOAT) {
        double val = arg.as.float_val;
        return value_make_float(val < 0 ? -val : val);
    } else {
        interpreter_error(interpreter, "abs() expects number", 0, 0);
        return value_make_undefined();
    }
}

Value builtin_sqrt(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 1) {
        interpreter_error(interpreter, "sqrt() expects 1 argument", 0, 0);
        return value_make_undefined();
    }
    
    Value arg = args[0];
    double val;
    
    if (arg.type == VAL_INT) {
        val = (double)arg.as.int_val;
    } else if (arg.type == VAL_FLOAT) {
        val = arg.as.float_val;
    } else {
        interpreter_error(interpreter, "sqrt() expects number", 0, 0);
        return value_make_undefined();
    }
    
    if (val < 0) {
        return value_make_nan();
    }
    
    return value_make_float(sqrt(val));
}

Value builtin_pow(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 2) {
        interpreter_error(interpreter, "pow() expects 2 arguments", 0, 0);
        return value_make_undefined();
    }
    
    double base, exponent;
    
    // Parse base
    if (args[0].type == VAL_INT) {
        base = (double)args[0].as.int_val;
    } else if (args[0].type == VAL_FLOAT) {
        base = args[0].as.float_val;
    } else {
        interpreter_error(interpreter, "pow() expects numbers", 0, 0);
        return value_make_undefined();
    }
    
    // Parse exponent
    if (args[1].type == VAL_INT) {
        exponent = (double)args[1].as.int_val;
    } else if (args[1].type == VAL_FLOAT) {
        exponent = args[1].as.float_val;
    } else {
        interpreter_error(interpreter, "pow() expects numbers", 0, 0);
        return value_make_undefined();
    }
    
    return value_make_float(pow(base, exponent));
}

// Built-in array functions
Value builtin_append(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 2) {
        interpreter_error(interpreter, "append() expects 2 arguments", 0, 0);
        return value_make_undefined();
    }
    
    if (args[0].type != VAL_ARRAY) {
        interpreter_error(interpreter, "append() first argument must be array", 0, 0);
        return value_make_undefined();
    }
    
    Value* array = &args[0];
    
    // Resize array if needed
    if (array->as.array.count >= array->as.array.capacity) {
        int new_capacity = array->as.array.capacity == 0 ? 4 : array->as.array.capacity * 2;
        array->as.array.elements = REALLOC(array->as.array.elements, Value, new_capacity);
        array->as.array.capacity = new_capacity;
    }
    
    // Copy the value to append
    array->as.array.elements[array->as.array.count] = args[1];
    array->as.array.count++;
    
    return args[0]; // Return the modified array
}

// Built-in string functions
Value builtin_upper(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 1) {
        interpreter_error(interpreter, "upper() expects 1 argument", 0, 0);
        return value_make_undefined();
    }
    
    if (args[0].type != VAL_STRING) {
        interpreter_error(interpreter, "upper() expects string", 0, 0);
        return value_make_undefined();
    }
    
    char* str = args[0].as.str_val;
    char* result = str_copy(str);
    
    for (int i = 0; result[i]; i++) {
        result[i] = toupper(result[i]);
    }
    
    return value_make_string(result);
}

Value builtin_lower(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    if (arg_count != 1) {
        interpreter_error(interpreter, "lower() expects 1 argument", 0, 0);
        return value_make_undefined();
    }
    
    if (args[0].type != VAL_STRING) {
        interpreter_error(interpreter, "lower() expects string", 0, 0);
        return value_make_undefined();
    }
    
    char* str = args[0].as.str_val;
    char* result = str_copy(str);
    
    for (int i = 0; result[i]; i++) {
        result[i] = tolower(result[i]);
    }
    
    return value_make_string(result);
}

// Built-in time function
Value builtin_time(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    (void)interpreter; // Marquer comme utilisé
    (void)args;
    (void)arg_count;
    
    // Solution portable avec time()
    return value_make_float((double)time(NULL));
}

// Built-in exit function
Value builtin_exit(SwiftFlowInterpreter* interpreter, Value* args, int arg_count) {
    (void)interpreter; // Marquer comme utilisé
    int exit_code = 0;
    
    if (arg_count > 0) {
        if (args[0].type == VAL_INT) {
            exit_code = (int)args[0].as.int_val;
        }
    }
    
    printf("Exiting SwiftFlow interpreter with code %d\n", exit_code);
    exit(exit_code);
    return value_make_null(); // Never reached
}

// Array of built-in functions
static Builtin builtins[] = {
    {"print", builtin_print, 0, -1},        // Variable arguments
    {"input", builtin_input, 0, 1},         // 0-1 arguments
    {"length", builtin_length, 1, 1},       // 1 argument
    {"typeof", builtin_typeof, 1, 1},       // 1 argument
    {"abs", builtin_abs, 1, 1},             // 1 argument
    {"sqrt", builtin_sqrt, 1, 1},           // 1 argument
    {"pow", builtin_pow, 2, 2},             // 2 arguments
    {"append", builtin_append, 2, 2},       // 2 arguments
    {"upper", builtin_upper, 1, 1},         // 1 argument
    {"lower", builtin_lower, 1, 1},         // 1 argument
    {"time", builtin_time, 0, 0},           // 0 arguments
    {"exit", builtin_exit, 0, 1},           // 0-1 arguments
    {NULL, NULL, 0, 0}                      // Sentinel
};

// ======================================================
// [SECTION] INTERPRETER CORE
// ======================================================

void interpreter_error(SwiftFlowInterpreter* interpreter, const char* message, int line, int column) {
    interpreter->had_error = true;
    FREE(interpreter->error_message);
    interpreter->error_message = str_copy(message);
    interpreter->error_line = line;
    interpreter->error_column = column;
    
    if (interpreter->debug_mode) {
        fprintf(stderr, "%s[ERROR]%s Line %d, Column %d: %s\n", 
                COLOR_RED, COLOR_RESET, line, column, message);
    }
}

SwiftFlowInterpreter* interpreter_new(void) {
    SwiftFlowInterpreter* interpreter = ALLOC(SwiftFlowInterpreter);
    CHECK_ALLOC(interpreter, "Interpreter allocation");
    
    interpreter->global_env = environment_new(NULL);
    interpreter->call_stack = NULL;
    interpreter->stack_size = 0;
    interpreter->stack_ptr = 0;
    
    interpreter->debug_mode = false;
    interpreter->verbose = false;
    
    interpreter->had_error = false;
    interpreter->error_message = NULL;
    interpreter->error_line = 0;
    interpreter->error_column = 0;
    
    interpreter->should_break = false;
    interpreter->should_continue = false;
    interpreter->should_return = false;
    
    // Initialize built-in functions
    interpreter_register_builtins(interpreter);
    
    return interpreter;
}

void interpreter_free(SwiftFlowInterpreter* interpreter) {
    if (!interpreter) return;
    
    environment_free(interpreter->global_env);
    FREE(interpreter->call_stack);
    FREE(interpreter->error_message);
    FREE(interpreter);
}

void interpreter_register_builtins(SwiftFlowInterpreter* interpreter) {
    for (int i = 0; builtins[i].name != NULL; i++) {
        // Create a function value for each built-in
        Value func_val;
        func_val.type = VAL_FUNCTION;
        func_val.as.function = NULL; // Built-in functions are special
        
        // Store the builtin index in the value
        environment_define(interpreter->global_env, builtins[i].name, func_val);
    }
}

Value call_builtin(SwiftFlowInterpreter* interpreter, const char* name, Value* args, int arg_count) {
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (str_equal(name, builtins[i].name)) {
            // Check argument count
            if (builtins[i].min_args >= 0 && arg_count < builtins[i].min_args) {
                interpreter_error(interpreter, "Too few arguments", 0, 0);
                return value_make_undefined();
            }
            if (builtins[i].max_args >= 0 && arg_count > builtins[i].max_args) {
                interpreter_error(interpreter, "Too many arguments", 0, 0);
                return value_make_undefined();
            }
            
            return builtins[i].func(interpreter, args, arg_count);
        }
    }
    
    interpreter_error(interpreter, "Unknown built-in function", 0, 0);
    return value_make_undefined();
}

Value interpreter_evaluate_binary(SwiftFlowInterpreter* interpreter, ASTNode* node, Environment* env) {
    Value left = interpreter_evaluate(interpreter, node->left, env);
    if (interpreter->had_error) {
        value_free(&left);
        return value_make_null();
    }
    
    Value right = interpreter_evaluate(interpreter, node->right, env);
    if (interpreter->had_error) {
        value_free(&left);
        value_free(&right);
        return value_make_null();
    }
    
    // Type checking and operation
    switch (node->op_type) {
        case TK_PLUS:
            // Addition or concatenation
            if (left.type == VAL_STRING || right.type == VAL_STRING) {
                char* left_str = value_to_string(left);
                char* right_str = value_to_string(right);
                char* result = str_format("%s%s", left_str, right_str);
                Value val = value_make_string(result);
                free(left_str);
                free(right_str);
                free(result);
                value_free(&left);
                value_free(&right);
                return val;
            } else if (left.type == VAL_INT && right.type == VAL_INT) {
                int64_t result = left.as.int_val + right.as.int_val;
                value_free(&left);
                value_free(&right);
                return value_make_int(result);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = (left.type == VAL_INT) ? (double)left.as.int_val : left.as.float_val;
                double r = (right.type == VAL_INT) ? (double)right.as.int_val : right.as.float_val;
                double result = l + r;
                value_free(&left);
                value_free(&right);
                return value_make_float(result);
            }
            break;
            
        case TK_MINUS:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                int64_t result = left.as.int_val - right.as.int_val;
                value_free(&left);
                value_free(&right);
                return value_make_int(result);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = (left.type == VAL_INT) ? (double)left.as.int_val : left.as.float_val;
                double r = (right.type == VAL_INT) ? (double)right.as.int_val : right.as.float_val;
                double result = l - r;
                value_free(&left);
                value_free(&right);
                return value_make_float(result);
            }
            break;
            
        case TK_MULT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                int64_t result = left.as.int_val * right.as.int_val;
                value_free(&left);
                value_free(&right);
                return value_make_int(result);
            } else if (left.type == VAL_FLOAT || right.type == VAL_FLOAT) {
                double l = (left.type == VAL_INT) ? (double)left.as.int_val : left.as.float_val;
                double r = (right.type == VAL_INT) ? (double)right.as.int_val : right.as.float_val;
                double result = l * r;
                value_free(&left);
                value_free(&right);
                return value_make_float(result);
            }
            break;
            
        case TK_DIV:
            if (right.type == VAL_INT && right.as.int_val == 0) {
                interpreter_error(interpreter, "Division by zero", node->line, node->column);
                value_free(&left);
                value_free(&right);
                return value_make_undefined();
            }
            if (right.type == VAL_FLOAT && right.as.float_val == 0.0) {
                interpreter_error(interpreter, "Division by zero", node->line, node->column);
                value_free(&left);
                value_free(&right);
                return value_make_undefined();
            }
            
            if (left.type == VAL_INT && right.type == VAL_INT) {
                // Integer division
                if (left.as.int_val % right.as.int_val == 0) {
                    int64_t result = left.as.int_val / right.as.int_val;
                    value_free(&left);
                    value_free(&right);
                    return value_make_int(result);
                } else {
                    // Promote to float
                    double result = (double)left.as.int_val / (double)right.as.int_val;
                    value_free(&left);
                    value_free(&right);
                    return value_make_float(result);
                }
            } else {
                double l = (left.type == VAL_INT) ? (double)left.as.int_val : left.as.float_val;
                double r = (right.type == VAL_INT) ? (double)right.as.int_val : right.as.float_val;
                double result = l / r;
                value_free(&left);
                value_free(&right);
                return value_make_float(result);
            }
            break;
            
        case TK_MOD:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                if (right.as.int_val == 0) {
                    interpreter_error(interpreter, "Modulo by zero", node->line, node->column);
                    value_free(&left);
                    value_free(&right);
                    return value_make_undefined();
                }
                int64_t result = left.as.int_val % right.as.int_val;
                value_free(&left);
                value_free(&right);
                return value_make_int(result);
            }
            break;
            
        case TK_EQ:
            return value_make_bool(value_equal(left, right));
            
        case TK_NEQ:
            return value_make_bool(!value_equal(left, right));
            
        case TK_GT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                bool result = left.as.int_val > right.as.int_val;
                value_free(&left);
                value_free(&right);
                return value_make_bool(result);
            } else {
                double l = (left.type == VAL_INT) ? (double)left.as.int_val : left.as.float_val;
                double r = (right.type == VAL_INT) ? (double)right.as.int_val : right.as.float_val;
                bool result = l > r;
                value_free(&left);
                value_free(&right);
                return value_make_bool(result);
            }
            break;
            
        case TK_LT:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                bool result = left.as.int_val < right.as.int_val;
                value_free(&left);
                value_free(&right);
                return value_make_bool(result);
            } else {
                double l = (left.type == VAL_INT) ? (double)left.as.int_val : left.as.float_val;
                double r = (right.type == VAL_INT) ? (double)right.as.int_val : right.as.float_val;
                bool result = l < r;
                value_free(&left);
                value_free(&right);
                return value_make_bool(result);
            }
            break;
            
        case TK_GTE:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                bool result = left.as.int_val >= right.as.int_val;
                value_free(&left);
                value_free(&right);
                return value_make_bool(result);
            } else {
                double l = (left.type == VAL_INT) ? (double)left.as.int_val : left.as.float_val;
                double r = (right.type == VAL_INT) ? (double)right.as.int_val : right.as.float_val;
                bool result = l >= r;
                value_free(&left);
                value_free(&right);
                return value_make_bool(result);
            }
            break;
            
        case TK_LTE:
            if (left.type == VAL_INT && right.type == VAL_INT) {
                bool result = left.as.int_val <= right.as.int_val;
                value_free(&left);
                value_free(&right);
                return value_make_bool(result);
            } else {
                double l = (left.type == VAL_INT) ? (double)left.as.int_val : left.as.float_val;
                double r = (right.type == VAL_INT) ? (double)right.as.int_val : right.as.float_val;
                bool result = l <= r;
                value_free(&left);
                value_free(&right);
                return value_make_bool(result);
            }
            break;
            
        case TK_AND:
            return value_make_bool(value_is_truthy(left) && value_is_truthy(right));
            
        case TK_OR:
            return value_make_bool(value_is_truthy(left) || value_is_truthy(right));
            
        default:
            interpreter_error(interpreter, "Unsupported binary operator", node->line, node->column);
            break;
    }
    
    value_free(&left);
    value_free(&right);
    return value_make_undefined();
}

Value interpreter_evaluate_unary(SwiftFlowInterpreter* interpreter, ASTNode* node, Environment* env) {
    Value right = interpreter_evaluate(interpreter, node->left, env);
    if (interpreter->had_error) {
        return value_make_null();
    }
    
    switch (node->op_type) {
        case TK_MINUS:
            if (right.type == VAL_INT) {
                int64_t result = -right.as.int_val;
                value_free(&right);
                return value_make_int(result);
            } else if (right.type == VAL_FLOAT) {
                double result = -right.as.float_val;
                value_free(&right);
                return value_make_float(result);
            }
            break;
            
        case TK_NOT:
            return value_make_bool(!value_is_truthy(right));
            
        default:
            interpreter_error(interpreter, "Unsupported unary operator", node->line, node->column);
            break;
    }
    
    value_free(&right);
    return value_make_undefined();
}

Value interpreter_evaluate_function_call(SwiftFlowInterpreter* interpreter, ASTNode* node, Environment* env) {
    // Get function name
    if (node->left->type != NODE_IDENT) {
        interpreter_error(interpreter, "Expected function name", node->line, node->column);
        return value_make_undefined();
    }
    
    char* func_name = node->left->data.name;
    
    // Evaluate arguments
    Value* args = NULL;
    int arg_count = 0;
    
    ASTNode* arg_node = node->right;
    while (arg_node) {
        arg_count++;
        args = REALLOC(args, Value, arg_count);
        args[arg_count - 1] = interpreter_evaluate(interpreter, arg_node, env);
        if (interpreter->had_error) {
            for (int i = 0; i < arg_count; i++) {
                value_free(&args[i]);
            }
            FREE(args);
            return value_make_undefined();
        }
        arg_node = arg_node->right; // Next argument in linked list
    }
    
    // Check if it's a built-in function
    for (int i = 0; builtins[i].name != NULL; i++) {
        if (str_equal(func_name, builtins[i].name)) {
            Value result = call_builtin(interpreter, func_name, args, arg_count);
            
            // Free arguments
            for (int i = 0; i < arg_count; i++) {
                value_free(&args[i]);
            }
            FREE(args);
            
            return result;
        }
    }
    
    // User-defined functions not implemented yet
    interpreter_error(interpreter, "Function not found", node->line, node->column);
    
    // Cleanup
    for (int i = 0; i < arg_count; i++) {
        value_free(&args[i]);
    }
    FREE(args);
    
    return value_make_undefined();
}

Value interpreter_evaluate_list(SwiftFlowInterpreter* interpreter, ASTNode* node, Environment* env) {
    Value array_val;
    array_val.type = VAL_ARRAY;
    array_val.as.array.count = 0;
    array_val.as.array.capacity = 0;
    array_val.as.array.elements = NULL;
    
    // Count elements
    int count = 0;
    ASTNode* elem = node->left;
    while (elem) {
        count++;
        elem = elem->right;
    }
    
    if (count > 0) {
        array_val.as.array.capacity = count;
        array_val.as.array.elements = ALLOC_ARRAY(Value, count);
        
        elem = node->left;
        int i = 0;
        while (elem) {
            array_val.as.array.elements[i] = interpreter_evaluate(interpreter, elem, env);
            if (interpreter->had_error) {
                // Cleanup partial array
                for (int j = 0; j < i; j++) {
                    value_free(&array_val.as.array.elements[j]);
                }
                FREE(array_val.as.array.elements);
                return value_make_undefined();
            }
            i++;
            elem = elem->right;
        }
        array_val.as.array.count = count;
    }
    
    return array_val;
}

Value interpreter_evaluate(SwiftFlowInterpreter* interpreter, ASTNode* node, Environment* env) {
    if (!node || interpreter->had_error) {
        return value_make_null();
    }
    
    if (interpreter->debug_mode) {
        printf("%s[DEBUG]%s Evaluating %s at %d:%d\n", 
               COLOR_CYAN, COLOR_RESET, node_type_to_string(node->type), node->line, node->column);
    }
    
    switch (node->type) {
        case NODE_INT:
            return value_make_int(node->data.int_val);
            
        case NODE_FLOAT:
            return value_make_float(node->data.float_val);
            
        case NODE_STRING:
            return value_make_string(node->data.str_val);
            
        case NODE_BOOL:
            return value_make_bool(node->data.bool_val);
            
        case NODE_NULL:
            return value_make_null();
            
        case NODE_UNDEFINED:
            return value_make_undefined();
            
        case NODE_NAN:
            return value_make_nan();
            
        case NODE_INF:
            return value_make_inf();
            
        case NODE_IDENT: {
            Value value = environment_get(env, node->data.name);
            if (value.type == VAL_UNDEFINED) {
                interpreter_error(interpreter, "Undefined variable", node->line, node->column);
            }
            return value;
        }
            
        case NODE_BINARY:
            return interpreter_evaluate_binary(interpreter, node, env);
            
        case NODE_UNARY:
            return interpreter_evaluate_unary(interpreter, node, env);
            
        case NODE_ASSIGN: {
            if (node->left->type != NODE_IDENT) {
                interpreter_error(interpreter, "Invalid assignment target", node->line, node->column);
                return value_make_undefined();
            }
            
            Value value = interpreter_evaluate(interpreter, node->right, env);
            if (interpreter->had_error) return value_make_undefined();
            
            bool success = environment_set(env, node->left->data.name, value);
            if (!success) {
                // Variable doesn't exist, create it
                environment_define(env, node->left->data.name, value);
            }
            
            return value;
        }
            
        case NODE_VAR_DECL:
        case NODE_NET_DECL:
        case NODE_CLOG_DECL:
        case NODE_DOS_DECL:
        case NODE_SEL_DECL:
        case NODE_CONST_DECL:
        case NODE_GLOBAL_DECL: {
            Value value;
            if (node->left) {
                value = interpreter_evaluate(interpreter, node->left, env);
                if (interpreter->had_error) return value_make_undefined();
            } else {
                value = value_make_undefined();
            }
            
            environment_define(env, node->data.name, value);
            return value;
        }
            
        case NODE_PRINT: {
            Value value = interpreter_evaluate(interpreter, node->left, env);
            if (interpreter->had_error) return value_make_null();
            
            value_print(value);
            printf("\n");
            value_free(&value);
            return value_make_null();
        }
            
        case NODE_INPUT: {
            if (node->data.input_op.prompt) {
                printf("%s", node->data.input_op.prompt);
                fflush(stdout);
            }
            
            char buffer[1024];
            if (fgets(buffer, sizeof(buffer), stdin)) {
                // Remove trailing newline
                size_t len = strlen(buffer);
                if (len > 0 && buffer[len-1] == '\n') {
                    buffer[len-1] = '\0';
                }
                return value_make_string(buffer);
            }
            return value_make_string("");
        }
            
        case NODE_IF: {
            Value condition = interpreter_evaluate(interpreter, node->left, env);
            if (interpreter->had_error) return value_make_null();
            
            Value result = value_make_null();
            
            if (value_is_truthy(condition)) {
                result = interpreter_evaluate(interpreter, node->right, env);
            } else if (node->third) {
                result = interpreter_evaluate(interpreter, node->third, env);
            }
            
            value_free(&condition);
            return result;
        }
            
        case NODE_WHILE: {
            Value result = value_make_null();
            
            while (true) {
                if (interpreter->should_break) {
                    interpreter->should_break = false;
                    break;
                }
                
                if (interpreter->should_continue) {
                    interpreter->should_continue = false;
                    continue;
                }
                
                Value condition = interpreter_evaluate(interpreter, node->left, env);
                if (interpreter->had_error) {
                    value_free(&result);
                    return value_make_null();
                }
                
                if (!value_is_truthy(condition)) {
                    value_free(&condition);
                    break;
                }
                
                value_free(&condition);
                value_free(&result);
                result = interpreter_evaluate(interpreter, node->right, env);
                
                if (interpreter->had_error || interpreter->should_return) {
                    break;
                }
            }
            
            return result;
        }
            
        case NODE_FOR: {
            Value result = value_make_null();
            
            // Create new scope for loop
            Environment* loop_env = environment_new(env);
            
            // Initialization
            if (node->data.loop.init) {
                interpreter_evaluate(interpreter, node->data.loop.init, loop_env);
                if (interpreter->had_error) {
                    environment_free(loop_env);
                    return value_make_null();
                }
            }
            
            while (true) {
                if (interpreter->should_break) {
                    interpreter->should_break = false;
                    break;
                }
                
                if (interpreter->should_continue) {
                    interpreter->should_continue = false;
                    // Still execute update
                    if (node->data.loop.update) {
                        interpreter_evaluate(interpreter, node->data.loop.update, loop_env);
                    }
                    continue;
                }
                
                // Condition
                if (node->data.loop.condition) {
                    Value condition = interpreter_evaluate(interpreter, node->data.loop.condition, loop_env);
                    if (interpreter->had_error) {
                        value_free(&result);
                        environment_free(loop_env);
                        return value_make_null();
                    }
                    
                    if (!value_is_truthy(condition)) {
                        value_free(&condition);
                        break;
                    }
                    value_free(&condition);
                }
                
                // Body
                value_free(&result);
                result = interpreter_evaluate(interpreter, node->data.loop.body, loop_env);
                
                if (interpreter->had_error || interpreter->should_return) {
                    break;
                }
                
                // Update
                if (node->data.loop.update) {
                    interpreter_evaluate(interpreter, node->data.loop.update, loop_env);
                    if (interpreter->had_error) {
                        value_free(&result);
                        environment_free(loop_env);
                        return value_make_null();
                    }
                }
            }
            
            environment_free(loop_env);
            return result;
        }
            
        case NODE_BREAK:
            interpreter->should_break = true;
            return value_make_null();
            
        case NODE_CONTINUE:
            interpreter->should_continue = true;
            return value_make_null();
            
        case NODE_RETURN: {
            Value value;
            if (node->left) {
                value = interpreter_evaluate(interpreter, node->left, env);
                if (interpreter->had_error) return value_make_undefined();
            } else {
                value = value_make_null();
            }
            interpreter->should_return = true;
            return value;
        }
            
        case NODE_BLOCK: {
            Value result = value_make_null();
            ASTNode* current = node->left;
            
            // Create new environment for block scope
            Environment* block_env = environment_new(env);
            
            while (current) {
                value_free(&result);
                result = interpreter_evaluate(interpreter, current, block_env);
                
                if (interpreter->had_error || interpreter->should_return || 
                    interpreter->should_break || interpreter->should_continue) {
                    break;
                }
                
                current = current->right; // Linked list of statements
            }
            
            // Clean up block environment
            environment_free(block_env);
            
            return result;
        }
            
        case NODE_LIST:
            return interpreter_evaluate_list(interpreter, node, env);
            
        case NODE_FUNC_CALL:
            return interpreter_evaluate_function_call(interpreter, node, env);
            
        case NODE_PASS:
            return value_make_null();
            
        case NODE_PROGRAM: {
            Value result = value_make_null();
            ASTNode* current = node->left;
            
            while (current) {
                value_free(&result);
                result = interpreter_evaluate(interpreter, current, env);
                
                if (interpreter->had_error || interpreter->should_return) {
                    break;
                }
                
                current = current->right;
            }
            
            return result;
        }
            
        default:
            interpreter_error(interpreter, "Unsupported node type", node->line, node->column);
            return value_make_undefined();
    }
}

Value interpreter_execute_block(SwiftFlowInterpreter* interpreter, ASTNode* block, Environment* env) {
    return interpreter_evaluate(interpreter, block, env);
}

int interpreter_run(SwiftFlowInterpreter* interpreter, ASTNode* ast) {
    if (!interpreter || !ast) return 1;
    
    // Reset interpreter state
    interpreter->had_error = false;
    FREE(interpreter->error_message);
    interpreter->error_message = NULL;
    interpreter->should_break = false;
    interpreter->should_continue = false;
    interpreter->should_return = false;
    
    // Evaluate the entire program
    Value result = interpreter_evaluate(interpreter, ast, interpreter->global_env);
    
    // Clean up result
    value_free(&result);
    
    return interpreter->had_error ? 1 : 0;
}

// ======================================================
// [SECTION] UTILITY FUNCTIONS
// ======================================================

void interpreter_dump_environment(SwiftFlowInterpreter* interpreter) {
    printf("%s=== Global Environment ===%s\n", COLOR_CYAN, COLOR_RESET);
    
    Environment* env = interpreter->global_env;
    for (int i = 0; i < env->variables.count; i++) {
        printf("  %s: ", env->variables.names[i]);
        value_print(env->variables.values[i]);
        printf("\n");
    }
}

void interpreter_dump_value(Value value) {
    printf("Value type: ");
    switch (value.type) {
        case VAL_INT: printf("INT"); break;
        case VAL_FLOAT: printf("FLOAT"); break;
        case VAL_BOOL: printf("BOOL"); break;
        case VAL_STRING: printf("STRING"); break;
        case VAL_NULL: printf("NULL"); break;
        case VAL_UNDEFINED: printf("UNDEFINED"); break;
        case VAL_NAN: printf("NAN"); break;
        case VAL_INF: printf("INF"); break;
        case VAL_ARRAY: printf("ARRAY"); break;
        case VAL_MAP: printf("MAP"); break;
        case VAL_FUNCTION: printf("FUNCTION"); break;
        case VAL_OBJECT: printf("OBJECT"); break;
        default: printf("UNKNOWN"); break;
    }
    
    printf(" = ");
    value_print(value);
    printf("\n");
}

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
            return value.as.float_val != 0.0;
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
// [SECTION] INTERPRETER CORE
// ======================================================

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

void interpreter_error(SwiftFlowInterpreter* interpreter, const char* message, int line, int column) {
    interpreter->had_error = true;
    FREE(interpreter->error_message);
    interpreter->error_message = str_copy(message);
    interpreter->error_line = line;
    interpreter->error_column = column;
}

Value interpreter_evaluate_binary(SwiftFlowInterpreter* interpreter, ASTNode* node, Environment* env) {
    Value left = interpreter_evaluate(interpreter, node->left, env);
    if (interpreter->had_error) return value_make_null();
    
    Value right = interpreter_evaluate(interpreter, node->right, env);
    if (interpreter->had_error) return value_make_null();
    
    // Type checking and operation
    switch (node->op_type) {
        case TK_PLUS:
            // Addition or concatenation
            if (left.type == VAL_STRING || right.type == VAL_STRING) {
                char* left_str = value_to_string(left);
                char* right_str = value_to_string(right);
                char* result = str_format("%s%s", left_str, right_str);
                free(left_str);
                free(right_str);
                Value val = value_make_string(result);
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
                return value_make_null();
            }
            if (right.type == VAL_FLOAT && right.as.float_val == 0.0) {
                interpreter_error(interpreter, "Division by zero", node->line, node->column);
                value_free(&left);
                value_free(&right);
                return value_make_null();
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
    return value_make_null();
}

Value interpreter_evaluate(SwiftFlowInterpreter* interpreter, ASTNode* node, Environment* env) {
    if (!node || interpreter->had_error) {
        return value_make_null();
    }
    
    if (interpreter->debug_mode) {
        printf("%s[DEBUG] Evaluating node %s at %d:%d%s\n", 
               COLOR_CYAN, node_type_to_string(node->type), node->line, node->column, COLOR_RESET);
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
            
        case NODE_IDENT: {
            Value value = environment_get(env, node->data.name);
            if (value.type == VAL_UNDEFINED) {
                interpreter_error(interpreter, "Undefined variable", node->line, node->column);
            }
            return value;
        }
            
        case NODE_NULL:
            return value_make_null();
            
        case NODE_UNDEFINED:
            return value_make_undefined();
            
        case NODE_BINARY:
            return interpreter_evaluate_binary(interpreter, node, env);
            
        case NODE_ASSIGN: {
            if (node->left->type != NODE_IDENT) {
                interpreter_error(interpreter, "Invalid assignment target", node->line, node->column);
                return value_make_null();
            }
            
            Value value = interpreter_evaluate(interpreter, node->right, env);
            if (interpreter->had_error) return value_make_null();
            
            bool success = environment_set(env, node->left->data.name, value);
            if (!success) {
                // Variable doesn't exist, create it
                environment_define(env, node->left->data.name, value);
            }
            
            return value;
        }
            
        case NODE_VAR_DECL: {
            Value value;
            if (node->left) {
                value = interpreter_evaluate(interpreter, node->left, env);
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
            
        case NODE_IF: {
            Value condition = interpreter_evaluate(interpreter, node->left, env);
            if (interpreter->had_error) return value_make_null();
            
            if (value_is_truthy(condition)) {
                value_free(&condition);
                return interpreter_evaluate(interpreter, node->right, env);
            } else if (node->third) {
                value_free(&condition);
                return interpreter_evaluate(interpreter, node->third, env);
            }
            
            value_free(&condition);
            return value_make_null();
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
            
            // Initialization
            if (node->data.loop.init) {
                interpreter_evaluate(interpreter, node->data.loop.init, env);
                if (interpreter->had_error) return value_make_null();
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
                        interpreter_evaluate(interpreter, node->data.loop.update, env);
                    }
                    continue;
                }
                
                // Condition
                if (node->data.loop.condition) {
                    Value condition = interpreter_evaluate(interpreter, node->data.loop.condition, env);
                    if (interpreter->had_error) {
                        value_free(&result);
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
                result = interpreter_evaluate(interpreter, node->data.loop.body, env);
                
                if (interpreter->had_error || interpreter->should_return) {
                    break;
                }
                
                // Update
                if (node->data.loop.update) {
                    interpreter_evaluate(interpreter, node->data.loop.update, env);
                    if (interpreter->had_error) {
                        value_free(&result);
                        return value_make_null();
                    }
                }
            }
            
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
            } else {
                value = value_make_null();
            }
            interpreter->should_return = true;
            return value;
        }
            
        case NODE_BLOCK: {
            Value result = value_make_null();
            ASTNode* current = node->left;
            
            while (current) {
                value_free(&result);
                result = interpreter_evaluate(interpreter, current, env);
                
                if (interpreter->had_error || interpreter->should_return || 
                    interpreter->should_break || interpreter->should_continue) {
                    break;
                }
                
                current = current->right; // Linked list of statements
            }
            
            return result;
        }
            
        case NODE_FUNC_CALL: {
            // TODO: Implement function calls
            printf("%s[WARNING] Function calls not yet implemented%s\n", COLOR_YELLOW, COLOR_RESET);
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
            
        default:
            interpreter_error(interpreter, "Unsupported node type", node->line, node->column);
            return value_make_null();
    }
}

Value interpreter_execute_block(SwiftFlowInterpreter* interpreter, ASTNode* block, Environment* env) {
    return interpreter_evaluate(interpreter, block, env);
}

void interpreter_register_builtins(SwiftFlowInterpreter* interpreter) {
    // Register basic I/O functions
    // These will be added to the interpreter's environment
}

int interpreter_run(SwiftFlowInterpreter* interpreter, ASTNode* ast) {
    if (!interpreter || !ast) return 1;
    
    // Reset interpreter state
    interpreter->had_error = false;
    interpreter->should_break = false;
    interpreter->should_continue = false;
    interpreter->should_return = false;
    
    // Evaluate the entire program
    Value result = interpreter_evaluate(interpreter, ast, interpreter->global_env);
    
    // Clean up result
    value_free(&result);
    
    return interpreter->had_error ? 1 : 0;
}

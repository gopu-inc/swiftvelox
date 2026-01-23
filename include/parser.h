#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "common.h"

// Forward declarations
struct SwiftFlowInterpreter;
struct Environment;

// Value operations
Value value_make_int(int64_t val);
Value value_make_float(double val);
Value value_make_bool(bool val);
Value value_make_string(const char* val);
Value value_make_null(void);
Value value_make_undefined(void);
Value value_make_nan(void);
Value value_make_inf(void);
void value_free(Value* value);
char* value_to_string(Value value);
void value_print(Value value);
bool value_is_truthy(Value value);
bool value_equal(Value a, Value b);

// Environment operations
struct Environment* environment_new(struct Environment* parent);
void environment_free(struct Environment* env);
void environment_define(struct Environment* env, const char* name, Value value);
bool environment_set(struct Environment* env, const char* name, Value value);
Value environment_get(struct Environment* env, const char* name);
bool environment_exists(struct Environment* env, const char* name);

// Interpreter operations
struct SwiftFlowInterpreter* interpreter_new(void);
void interpreter_free(struct SwiftFlowInterpreter* interpreter);
int interpreter_run(struct SwiftFlowInterpreter* interpreter, ASTNode* ast);
Value interpreter_evaluate(struct SwiftFlowInterpreter* interpreter, ASTNode* node, struct Environment* env);
Value interpreter_execute_block(struct SwiftFlowInterpreter* interpreter, ASTNode* block, struct Environment* env);
void interpreter_error(struct SwiftFlowInterpreter* interpreter, const char* message, int line, int column);
void interpreter_register_builtins(struct SwiftFlowInterpreter* interpreter);

// Utility functions
void interpreter_dump_environment(struct SwiftFlowInterpreter* interpreter);
void interpreter_dump_value(Value value);

#endif // INTERPRETER_H

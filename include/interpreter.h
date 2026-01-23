#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "common.h"

// Interpreter structures forward declarations
typedef struct SwiftFlowInterpreter SwiftFlowInterpreter;
typedef struct Environment Environment;
typedef struct Value Value;

// Interpreter API
SwiftFlowInterpreter* interpreter_new(void);
void interpreter_free(SwiftFlowInterpreter* interpreter);
int interpreter_run(SwiftFlowInterpreter* interpreter, ASTNode* ast);
Value interpreter_evaluate(SwiftFlowInterpreter* interpreter, ASTNode* node, Environment* env);
Value interpreter_execute_block(SwiftFlowInterpreter* interpreter, ASTNode* block, Environment* env);
void interpreter_error(SwiftFlowInterpreter* interpreter, const char* message, int line, int column);

// Built-in functions registration
void interpreter_register_builtins(SwiftFlowInterpreter* interpreter);

// Value operations
char* value_to_raw_string(Value value);  // <-- AJOUTER CETTE LIGNE
char* value_to_string(Value value);
void value_print(Value value);
bool value_is_truthy(Value value);
bool value_equal(Value a, Value b);

// Environment operations
Environment* environment_new(Environment* parent);
void environment_free(Environment* env);
void environment_define(Environment* env, const char* name, Value value);
bool environment_set(Environment* env, const char* name, Value value);
Value environment_get(Environment* env, const char* name);
bool environment_exists(Environment* env, const char* name);

#endif // INTERPRETER_H

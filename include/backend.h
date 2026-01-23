#ifndef BACKEND_H
#define BACKEND_H

#include "common.h"
#include "ast.h"

// Backend targets
typedef enum {
    BACKEND_LLVM,
    BACKEND_NASM,
    BACKEND_INTERPRETER
} BackendTarget;

// Backend context
typedef struct {
    BackendTarget target;
    FILE* output_file;
    char* output_filename;
    int label_counter;
    int temp_counter;
    SymbolTable* symbol_table;
} BackendContext;

// Backend functions
BackendContext* backend_create(BackendTarget target, const char* filename);
void backend_free(BackendContext* context);
void backend_compile(BackendContext* context, ASTNode* ast);

// LLVM backend
void llvm_emit_prologue(BackendContext* context);
void llvm_emit_epilogue(BackendContext* context);
void llvm_emit_node(BackendContext* context, ASTNode* node);

// NASM backend
void nasm_emit_prologue(BackendContext* context);
void nasm_emit_epilogue(BackendContext* context);
void nasm_emit_node(BackendContext* context, ASTNode* node);

// Interpreter
typedef struct {
    ASTNode* ast;
    SymbolTable* global_symbols;
    FILE* stdout;
    FILE* stdin;
} Interpreter;

Interpreter* interpreter_create(ASTNode* ast);
void interpreter_free(Interpreter* interpreter);
int interpreter_execute(Interpreter* interpreter);
Value interpreter_evaluate(Interpreter* interpreter, ASTNode* node);
Value interpreter_call_function(Interpreter* interpreter, ASTNode* function, Value* args, int arg_count);

#endif // BACKEND_H

#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "parser.h"
#include "value.h"

// Environnement
Environment* new_environment(Environment* enclosing);
void env_define(Environment* env, char* name, Value value);
int env_get(Environment* env, char* name, Value* out);

// Évaluation
Value eval(ASTNode* node, Environment* env);
Value eval_block(ASTNode* node, Environment* env);
Value eval_binary(ASTNode* node, Environment* env);
Value eval_unary(ASTNode* node, Environment* env);
Value eval_ternary(ASTNode* node, Environment* env);
Value eval_assignment(ASTNode* node, Environment* env);
Value eval_call(ASTNode* node, Environment* env);
Value eval_array(ASTNode* node, Environment* env);
Value eval_object(ASTNode* node, Environment* env);
Value eval_identifier(ASTNode* node, Environment* env);
Value eval_if(ASTNode* node, Environment* env);
Value eval_while(ASTNode* node, Environment* env);
Value eval_function(ASTNode* node, Environment* env);

// Fonctions utilitaires
void fatal_error(const char* fmt, ...);

// Variables globales
extern Environment* global_env;

#endif

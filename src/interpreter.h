#ifndef INTERPRETER_H
#define INTERPRETER_H

#include "value.h"
#include "parser.h"

// Environnement
typedef struct Environment {
    struct Environment* enclosing;
    char** names;
    Value* values;
    int count;
    int capacity;
} Environment;

Environment* new_environment(Environment* enclosing);
void env_define(Environment* env, char* name, Value value);
int env_get(Environment* env, char* name, Value* out);

// Évaluation - ajoute le prototype
Value eval(ASTNode* node, Environment* env);

// Variables globales
extern Environment* global_env;

#endif

#ifndef NATIVE_H
#define NATIVE_H

#include "interpreter.h"

// Prototypes des fonctions natives
Value native_print(Value* args, int count, Environment* env);
Value native_http_run(Value* args, int count, Environment* env);

// Enregistrement des fonctions natives
void register_natives(Environment* env);

#endif

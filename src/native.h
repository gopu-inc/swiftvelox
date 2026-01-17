#ifndef NATIVE_H
#define NATIVE_H

#include "interpreter.h"

// Fonctions natives
Value native_print(Value* args, int count, Environment* env);
Value native_log(Value* args, int count, Environment* env);
Value native_input(Value* args, int count, Environment* env);
Value native_clock(Value* args, int count, Environment* env);
Value native_typeof(Value* args, int count, Environment* env);
Value native_length(Value* args, int count, Environment* env);
Value native_range(Value* args, int count, Environment* env);
Value native_map(Value* args, int count, Environment* env);
Value native_filter(Value* args, int count, Environment* env);
Value native_reduce(Value* args, int count, Environment* env);
Value native_http_run(Value* args, int count, Environment* env);
Value native_fs_read(Value* args, int count, Environment* env);
Value native_fs_write(Value* args, int count, Environment* env);
Value native_math_sqrt(Value* args, int count, Environment* env);
Value native_math_pow(Value* args, int count, Environment* env);
Value native_assert(Value* args, int count, Environment* env);

// Enregistrement des fonctions natives
void register_natives(Environment* env);

#endif

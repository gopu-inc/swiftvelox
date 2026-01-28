#ifndef SWIFT_STDLIB_H
#define SWIFT_STDLIB_H

#include "common.h"

// Math
double std_math_calc(int op_type, double val1, double val2);

// String
char* std_str_upper(const char* s);
char* std_str_lower(const char* s);
char* std_str_replace(const char* orig, const char* rep, const char* with);
char* std_str_sub(const char* s, int start, int len);

// Time
double std_time_now(void);
void std_time_sleep(double seconds);

// Encoding
char* std_b64_encode(const char* data);
char* std_b64_decode(const char* data);

// Env
char* std_env_get(const char* key);
void std_env_set(const char* key, const char* value);
char* std_env_os(void);

// Path
char* std_path_basename(const char* path);
char* std_path_dirname(const char* path);
char* std_path_join(const char* p1, const char* p2);
char* std_path_abs(const char* path);

// String Extensions
char* std_str_trim(const char* s);
int std_str_contains(const char* haystack, const char* needle);

#endif

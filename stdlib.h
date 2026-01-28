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

#endif
